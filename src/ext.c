/* Extension dependent execution.
   Copyright (C) 1994, 1995 The Free Software Foundation

   Written by: 1995 Jakub Jelinek
               1994 Miguel de Icaza

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <string.h>
#include <errno.h>

#include "global.h"
#include "tty.h"
#include "user.h"
#include "main.h"
#include "wtools.h"
#include "ext.h"
#include "view.h"

#include "cons.saver.h"
#include "layout.h"

/* If set, we execute the file command to check the file type */
int use_file_to_check_type = 1;

/* This variable points to a copy of the mc.ext file in memory
 * With this we avoid loading/parsing the file each time we
 * need it
 */
static char *data = NULL;

void
flush_extension_file (void)
{
    if (data) {
	g_free (data);
	data = NULL;
    }

}

typedef char *(*quote_func_t) (const char *name, int i);

static void
exec_extension (const char *filename, const char *data, int *move_dir,
		int start_line)
{
    char *file_name;
    int cmd_file_fd;
    FILE *cmd_file;
    char *cmd = NULL;
    int expand_prefix_found = 0;
    int parameter_found = 0;
    char prompt[80];
    int run_view = 0;
    int def_hex_mode = default_hex_mode, changed_hex_mode = 0;
    int def_nroff_flag = default_nroff_flag, changed_nroff_flag = 0;
    int written_nonspace = 0;
    int is_cd = 0;
    char buffer[1024];
    char *p = 0;
    char *localcopy = NULL;
    int do_local_copy;
    time_t localmtime = 0;
    struct stat mystat;
    quote_func_t quote_func = name_quote;

    g_return_if_fail (filename != NULL);
    g_return_if_fail (data != NULL);

    /* Avoid making a local copy if we are doing a cd */
    if (!vfs_file_is_local (filename))
	do_local_copy = 1;
    else
	do_local_copy = 0;

    /*
     * All commands should be run in /bin/sh regardless of user shell.
     * To do that, create temporary shell script and run it.
     * Sometimes it's not needed (e.g. for %cd and %view commands),
     * but it's easier to create it anyway.
     */
    cmd_file_fd = mc_mkstemps (&file_name, "mcext", SCRIPT_SUFFIX);

    if (cmd_file_fd == -1) {
	message (1, MSG_ERROR,
		 _(" Cannot create temporary command file \n %s "),
		 unix_error_string (errno));
	return;
    }
    cmd_file = fdopen (cmd_file_fd, "w");
    fputs ("#! /bin/sh\n", cmd_file);

    prompt[0] = 0;
    for (; *data && *data != '\n'; data++) {
	if (parameter_found) {
	    if (*data == '}') {
		char *parameter;
		parameter_found = 0;
		parameter = input_dialog (_(" Parameter "), prompt, "");
		if (!parameter) {
		    /* User canceled */
		    fclose (cmd_file);
		    unlink (file_name);
		    if (localcopy) {
			mc_ungetlocalcopy (filename, localcopy, 0);
		    }
		    g_free (file_name);
		    return;
		}
		fputs (parameter, cmd_file);
		written_nonspace = 1;
		g_free (parameter);
	    } else {
		int len = strlen (prompt);

		if (len < sizeof (prompt) - 1) {
		    prompt[len] = *data;
		    prompt[len + 1] = 0;
		}
	    }
	} else if (expand_prefix_found) {
	    expand_prefix_found = 0;
	    if (*data == '{')
		parameter_found = 1;
	    else {
		int i = check_format_view (data);
		char *v;

		if (i) {
		    data += i - 1;
		    run_view = 1;
		} else if ((i = check_format_cd (data)) > 0) {
		    is_cd = 1;
		    quote_func = fake_name_quote;
		    do_local_copy = 0;
		    p = buffer;
		    data += i - 1;
		} else if ((i = check_format_var (data, &v)) > 0 && v) {
		    fputs (v, cmd_file);
		    g_free (v);
		    data += i;
		} else {
		    char *text;

		    if (*data == 'f') {
			if (do_local_copy) {
			    localcopy = mc_getlocalcopy (filename);
			    if (localcopy == NULL) {
				fclose (cmd_file);
				unlink (file_name);
				g_free (file_name);
				return;
			    }
			    mc_stat (localcopy, &mystat);
			    localmtime = mystat.st_mtime;
			    text = (*quote_func) (localcopy, 0);
			} else {
			    text = (*quote_func) (filename, 0);
			}
		    } else
			text = expand_format (NULL, *data, !is_cd);
		    if (!is_cd)
			fputs (text, cmd_file);
		    else {
			strcpy (p, text);
			p = strchr (p, 0);
		    }
		    g_free (text);
		    written_nonspace = 1;
		}
	    }
	} else {
	    if (*data == '%')
		expand_prefix_found = 1;
	    else {
		if (*data != ' ' && *data != '\t')
		    written_nonspace = 1;
		if (is_cd)
		    *(p++) = *data;
		else
		    fputc (*data, cmd_file);
	    }
	}
    }				/* for */

    /*
     * Make the script remove itself when it finishes.
     * Don't do it for the viewer - it may need to rerun the script,
     * so we clean up after calling view().
     */
    if (!run_view) {
	fprintf (cmd_file, "\n/bin/rm -f %s\n", file_name);
    }

    fclose (cmd_file);

    if ((run_view && !written_nonspace) || is_cd) {
	unlink (file_name);
	g_free (file_name);
	file_name = NULL;
    } else {
	int cmd_len = strlen (file_name) + 10;

	/* Set executable flag on the command file ... */
	chmod (file_name, S_IRWXU);
	/* ... but don't rely on it - run /bin/sh explicitly */
	cmd = g_malloc (cmd_len);
	g_snprintf (cmd, cmd_len, "/bin/sh %s", file_name);
    }

    if (run_view) {
	altered_hex_mode = 0;
	altered_nroff_flag = 0;
	if (def_hex_mode != default_hex_mode)
	    changed_hex_mode = 1;
	if (def_nroff_flag != default_nroff_flag)
	    changed_nroff_flag = 1;

	/* If we've written whitespace only, then just load filename
	 * into view
	 */
	if (written_nonspace) {
	    view (cmd, filename, move_dir, start_line);
	    unlink (file_name);
	} else {
	    view (0, filename, move_dir, start_line);
	}
	if (changed_hex_mode && !altered_hex_mode)
	    default_hex_mode = def_hex_mode;
	if (changed_nroff_flag && !altered_nroff_flag)
	    default_nroff_flag = def_nroff_flag;
	repaint_screen ();
    } else if (is_cd) {
	char *q;
	*p = 0;
	p = buffer;
/*	while (*p == ' ' && *p == '\t')
 *	    p++;
 */
	/* Search last non-space character. Start search at the end in order
	   not to short filenames containing spaces. */
	q = p + strlen (p) - 1;
	while (q >= p && (*q == ' ' || *q == '\t'))
	    q--;
	q[1] = 0;
	do_cd (p, cd_parse_command);
    } else {
	shell_execute (cmd, EXECUTE_INTERNAL);
	if (console_flag) {
	    handle_console (CONSOLE_SAVE);
	    if (output_lines && keybar_visible) {
		show_console_contents (output_start_y,
				       LINES - keybar_visible -
				       output_lines - 1,
				       LINES - keybar_visible - 1);

	    }
	}
    }

    g_free (file_name);
    g_free (cmd);

    if (localcopy) {
	mc_stat (localcopy, &mystat);
	mc_ungetlocalcopy (filename, localcopy,
			   localmtime != mystat.st_mtime);
    }
}

#ifdef FILE_L
#   define FILE_CMD "file -L "
#else
#   define FILE_CMD "file "
#endif

/*
 * Run the "file" command on the local file.
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 */
static int
get_file_type_local (char *filename, char *buf, int buflen)
{
    int read_bytes = 0;

    char *tmp = name_quote (filename, 0);
    char *command = g_strconcat (FILE_CMD, tmp, NULL);
    FILE *f = popen (command, "r");

    g_free (tmp);
    g_free (command);
    if (f != NULL) {
	read_bytes = (fgets (buf, buflen - 1, f)
		      != NULL);
	if (read_bytes == 0)
	    buf[0] = 0;
	pclose (f);
#ifdef SCO_FLAVOR
	/*
	 **       SCO 3.2 does has a buggy pclose(), so
	 **       <command> become zombie (alex)
	 */
	waitpid (-1, NULL, WNOHANG);
#endif				/* SCO_FLAVOR */
    } else {
	return -1;
    }

    return (read_bytes > 0);
}


#ifdef FILE_STDIN
/*
 * Read file through VFS and feed is to the "file" command.
 * Return 1 if the data is valid, 0 otherwise, -1 for fatal errors.
 */
static int
get_file_type_pipe (char *filename, char *buf, int buflen)
{
    int read_bytes = 0;

    int pipehandle, remotehandle;
    pid_t p;

    remotehandle = mc_open (filename, O_RDONLY);
    if (remotehandle != -1) {
	/* 8192 is HOWMANY hardcoded value in the file-3.14
	 * sources. Tell me if any other file uses larger
	 * chunk from beginning
	 */
	pipehandle =
	    mc_doublepopen (remotehandle, 8192, &p, "file", "file", "-",
			    NULL);
	if (pipehandle != -1) {
	    int i;
	    while ((i =
		    read (pipehandle, buf + read_bytes,
			  buflen - 1 - read_bytes)) > 0)
		read_bytes += i;
	    mc_doublepclose (pipehandle, p);
	    buf[read_bytes] = 0;
	}
	mc_close (remotehandle);
    } else {
	return -1;
    }

    return (read_bytes > 0);
}
#endif				/* FILE_STDIN */


/*
 * Invoke the "file" command on the file and match its output against PTR.
 * have_type is a flag that is set if we already have tried to determine
 * the type of that file.
 * Return 1 for match, 0 otherwise.
 */
static int
regex_check_type (char *filename, int file_len, char *ptr, int *have_type)
{
    int found = 0;
    int islocal;

    /* Following variables are valid if *have_type is 1 */
    static char content_string[2048];
    static int content_shift = 0;
    static int got_data = 0;

    if (!use_file_to_check_type) {
	return 0;
    }

    islocal = vfs_file_is_local (filename);

    if (!*have_type) {
	/* Don't repeate even unsuccessful checks */
	*have_type = 1;

	if (islocal) {
	    got_data =
		get_file_type_local (filename, content_string,
				     sizeof (content_string));
	} else
#ifdef FILE_STDIN
	{
	    got_data =
		get_file_type_pipe (filename, content_string,
				    sizeof (content_string));
	}
#else
	    /* Cannot use pipe, must make a local copy, not yet supported */
	    return 0;
#endif				/* !FILE_STDIN */

	if (got_data > 0) {
	    char *pp;

	    /* Paranoid termination */
	    content_string[sizeof (content_string) - 1] = 0;

	    if ((pp = strchr (content_string, '\n')) != 0)
		*pp = 0;

	    if (islocal && !strncmp (content_string, filename, file_len)) {
		/* Skip "filename: " */
		content_shift = file_len;
		if (content_string[content_shift] == ':')
		    for (content_shift++;
			 content_string[content_shift] == ' ';
			 content_shift++);
	    } else if (!islocal
		       && !strncmp (content_string, "standard input:",
				    15)) {
		/* Skip "standard input: " */
		for (content_shift = 15;
		     content_string[content_shift] == ' ';
		     content_shift++);
	    }
	} else {
	    /* No data */
	    content_string[0] = 0;
	}
    }

    if (got_data == -1) {
	return -1;
    }

    if (content_string && content_string[0]
	&& regexp_match (ptr, content_string + content_shift,
			 match_normal)) {
	found = 1;
    }

    return found;
}


/* The second argument is action, i.e. Open, View or Edit
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
regex_command (char *filename, char *action, int *move_dir)
{
    char *p, *q, *r, c;
    int file_len = strlen (filename);
    int found = 0;
    int error_flag = 0;
    int ret = 0;
    int old_patterns;
    struct stat mystat;
    int view_at_line_number;
    char *include_target;
    int include_target_len;
    int have_type = 0;		/* Flag used by regex_check_type() */

    /* Check for the special View:%d parameter */
    if (strncmp (action, "View:", 5) == 0) {
	view_at_line_number = atoi (action + 5);
	action[4] = 0;
    } else {
	view_at_line_number = 0;
    }

    if (data == NULL) {
	char *extension_file;
	int mc_user_ext = 1;
	int home_error = 0;

	extension_file = concat_dir_and_file (home_dir, MC_USER_EXT);
	if (!exist_file (extension_file)) {
	    g_free (extension_file);
	  check_stock_mc_ext:
	    extension_file = concat_dir_and_file (mc_home, MC_LIB_EXT);
	    mc_user_ext = 0;
	}
	data = load_file (extension_file);
	g_free (extension_file);
	if (data == NULL)
	    return 0;

	if (!strstr (data, "default/")) {
	    if (!strstr (data, "regex/") && !strstr (data, "shell/")
		&& !strstr (data, "type/")) {
		g_free (data);
		data = NULL;
		if (mc_user_ext) {
		    home_error = 1;
		    goto check_stock_mc_ext;
		} else {
		    char *msg;
		    char *msg2;
		    msg =
			g_strconcat (" ", mc_home, MC_LIB_EXT,
				     _(" file error "), NULL);
		    msg2 =
			g_strconcat (_("Format of the "), mc_home,
				     _("mc.ext file has changed\n"
				       "with version 3.0. It seems that installation\n"
				       "failed. Please fetch a fresh new copy from the\n"
				       "Midnight Commander package."),
				     NULL);
		    message (1, msg, "%s", msg2);
		    g_free (msg);
		    g_free (msg2);
		    return 0;
		}
	    }
	}
	if (home_error) {
	    char *msg;
	    char *msg2;
	    msg =
		g_strconcat (" ~/", MC_USER_EXT, _(" file error "), NULL);
	    msg2 =
		g_strconcat (_("Format of the "), "~/", MC_USER_EXT,
			     _(" file has changed\n"
			       "with version 3.0. You may want either to\n"
			       "copy it from "), mc_home,
			     _("mc.ext or use that\n"
			       "file as an example of how to write it.\n"),
			     mc_home,
			     _("mc.ext will be used for this moment."),
			     NULL);
	    message (1, msg, "%s", msg2);
	    g_free (msg);
	    g_free (msg2);
	}
    }
    mc_stat (filename, &mystat);

    old_patterns = easy_patterns;
    easy_patterns = 0;		/* Real regular expressions are needed :) */
    include_target = NULL;
    include_target_len = 0;
    for (p = data; *p; p++) {
	for (q = p; *q == ' ' || *q == '\t'; q++);
	if (*q == '\n' || !*q)
	    p = q;		/* empty line */
	if (*p == '#')		/* comment */
	    while (*p && *p != '\n')
		p++;
	if (*p == '\n')
	    continue;
	if (!*p)
	    break;
	if (p == q) {		/* i.e. starts in the first column, should be
				 * keyword/descNL
				 */
	    found = 0;
	    q = strchr (p, '\n');
	    if (q == NULL)
		q = strchr (p, 0);
	    c = *q;
	    *q = 0;
	    if (include_target) {
		if ((strncmp (p, "include/", 8) == 0)
		    && (strncmp (p + 8, include_target, include_target_len)
			== 0))
		    found = 1;
	    } else if (!strncmp (p, "regex/", 6)) {
		p += 6;
		/* Do not transform shell patterns, you can use shell/ for
		 * that
		 */
		if (regexp_match (p, filename, match_normal))
		    found = 1;
	    } else if (!strncmp (p, "directory/", 10)) {
		if (S_ISDIR (mystat.st_mode)
		    && regexp_match (p + 10, filename, match_normal))
		    found = 1;
	    } else if (!strncmp (p, "shell/", 6)) {
		p += 6;
		if (*p == '.' && file_len >= (q - p)) {
		    if (!strncmp (p, filename + file_len - (q - p), q - p))
			found = 1;
		} else {
		    if (q - p == file_len && !strncmp (p, filename, q - p))
			found = 1;
		}
	    } else if (!strncmp (p, "type/", 5)) {
		int res;
		p += 5;
		res = regex_check_type (filename, file_len, p, &have_type);
		if (res == 1)
		    found = 1;
		if (res == -1)
		    error_flag = 1;	/* leave it if file cannot be opened */
	    } else if (!strncmp (p, "default/", 8)) {
		found = 1;
	    }
	    *q = c;
	    p = q;
	    if (!*p)
		break;
	} else {		/* List of actions */
	    p = q;
	    q = strchr (p, '\n');
	    if (q == NULL)
		q = strchr (p, 0);
	    if (found && !error_flag) {
		r = strchr (p, '=');
		if (r != NULL) {
		    c = *r;
		    *r = 0;
		    if (strcmp (p, "Include") == 0) {
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
		    if (!strcmp (action, p)) {
			*r = c;
			for (p = r + 1; *p == ' ' || *p == '\t'; p++);

			/* Empty commands just stop searching
			 * through, they don't do anything
			 *
			 * We need to copy the filename because exec_extension
			 * may end up invoking update_panels thus making the
			 * filename parameter invalid (ie, most of the time,
			 * we get filename as a pointer from cpanel->dir).
			 */
			if (p < q) {
			    char *filename_copy = g_strdup (filename);

			    exec_extension (filename_copy, r + 1, move_dir,
					    view_at_line_number);
			    g_free (filename_copy);

			    ret = 1;
			}
			break;
		    } else
			*r = c;
		}
	    }
	    p = q;
	    if (!*p)
		break;
	}
    }
    easy_patterns = old_patterns;
    if (error_flag)
	return -1;
    return ret;
}
