/* User Menu implementation
   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007 Free Software Foundation, Inc.
   
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file user.c
 *  \brief Source: user menu implementation
 */

#include <config.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/search.h"
#include "lib/vfs/mc-vfs/vfs.h"
#include "lib/strutil.h"

#include "src/editor/edit.h"		/* WEdit, BLOCK_FILE */
#include "src/viewer/mcviewer.h" /* for default_* externs */

#include "dir.h"
#include "panel.h"
#include "main.h"
#include "user.h"
#include "layout.h"
#include "execute.h"
#include "setup.h"
#include "history.h"


/* For the simple listbox manager */
#include "dialog.h"
#include "widget.h"
#include "wtools.h"

#define MAX_ENTRIES 16
#define MAX_ENTRY_LEN 60

static int debug_flag = 0;
static int debug_error = 0;
static char *menu = NULL;

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
int check_format_view (const char *p)
{
    const char *q = p;
    if (!strncmp (p, "view", 4)) {
    	q += 4;
    	if (*q == '{'){
    	    for (q++;*q && *q != '}';q++) {
    	    	if (!strncmp (q, "ascii", 5)) {
    	    	    mcview_default_hex_mode = 0;
    	    	    q += 4;
    	    	} else if (!strncmp (q, "hex", 3)) {
    	    	    mcview_default_hex_mode = 1;
    	    	    q += 2;
    	    	} else if (!strncmp (q, "nroff", 5)) {
    	    	    mcview_default_nroff_flag = 1;
    	    	    q += 4;
    	    	} else if (!strncmp (q, "unform", 6)) {
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

int check_format_cd (const char *p)
{
    return (strncmp (p, "cd", 2)) ? 0 : 3;
}

/* Check if p has a "^var\{var-name\}" */
/* Returns the number of skipped characters (zero on not found) */
/* V will be set to the expanded variable name */
int check_format_var (const char *p, char **v)
{
    const char *q = p;
    char *var_name;
    const char *value;
    const char *dots = 0;
    
    *v = 0;
    if (!strncmp (p, "var{", 4)){
	for (q += 4; *q && *q != '}'; q++){
	    if (*q == ':')
		dots = q+1;
	}
	if (!*q)
	    return 0;

	if (!dots || dots == q+5){
	    message (D_ERROR,
		     _(" Format error on file Extensions File "),
		     !dots ? _(" The %%var macro has no default ")
		     :       _(" The %%var macro has no variable "));
	    return 0;
	}
	
	/* Copy the variable name */
	var_name = g_strndup (p + 4, dots-2 - (p+3));

	value = getenv (var_name);
	g_free (var_name);
	if (value){
	    *v = g_strdup (value);
	    return q-p;
	}	
	var_name = g_strndup (dots, q - dots);
	*v = var_name;
	return q-p;
    }
    return 0;
}

/* strip file's extension */
static char *
strip_ext(char *ss)
{
    register char *s = ss;
    char *e = NULL;
    while(*s) {
        if(*s == '.') e = s;
        if(*s == PATH_SEP && e) e=NULL; /* '.' in *directory* name */
        s++;
    }
    if(e) *e = 0;
    return ss;
}

char *
expand_format (struct WEdit *edit_widget, char c, gboolean do_quote)
{
    WPanel *panel = NULL;
    char *(*quote_func) (const char *, int);
    char *fname = NULL;
    char *result;
    char c_lc;

#ifndef USE_INTERNAL_EDIT
    (void) edit_widget;
#endif

    if (c == '%')
	return g_strdup ("%");

    if (edit_one_file == NULL) {
	if (g_ascii_islower ((gchar) c))
	    panel = current_panel;
	else {
	    if (get_other_type () != view_listing)
		return g_strdup ("");
	    panel = other_panel;
	}
	fname = panel->dir.list[panel->selected].fname;
    }
#ifdef USE_INTERNAL_EDIT
    else
	fname = str_unconst (edit_get_file_name (edit_widget));
#endif

    if (do_quote)
	quote_func = name_quote;
    else
	quote_func = fake_name_quote;

    c_lc = g_ascii_tolower ((gchar) c);

    switch (c_lc) {
    case 'f':
    case 'p':
	return (*quote_func) (fname, 0);
    case 'x':
	return (*quote_func) (extension (fname), 0);
    case 'd':
	{
	    char *cwd;
	    char *qstr;

	    cwd = g_malloc(MC_MAXPATHLEN + 1);

	    if (panel)
		g_strlcpy(cwd, panel->cwd, MC_MAXPATHLEN + 1);
	    else
		mc_get_current_wd(cwd, MC_MAXPATHLEN + 1);

	    qstr = (*quote_func) (cwd, 0);

	    g_free (cwd);

	    return qstr;
	}
    case 'i':			/* indent equal number cursor position in line */
#ifdef USE_INTERNAL_EDIT
	if (edit_widget)
	    return g_strnfill (edit_get_curs_col (edit_widget), ' ');
#endif
	break;
    case 'y':			/* syntax type */
#ifdef USE_INTERNAL_EDIT
	if (edit_widget) {
	    const char *syntax_type = edit_get_syntax_type (edit_widget);
	    if (syntax_type != NULL)
		return g_strdup (syntax_type);
	}
#endif
	break;
    case 'k':			/* block file name */
    case 'b':			/* block file name / strip extension */  {
#ifdef USE_INTERNAL_EDIT
	    if (edit_widget) {
		char *file = concat_dir_and_file (home_dir, EDIT_BLOCK_FILE);
		fname = (*quote_func) (file, 0);
		g_free (file);
		return fname;
	    }
#endif
	    if (c_lc == 'b')
		return strip_ext ((*quote_func) (fname, 0));
	    break;
	}
    case 'n':			/* strip extension in editor */
#ifdef USE_INTERNAL_EDIT
	if (edit_widget)
	    return strip_ext ((*quote_func) (fname, 0));
#endif
	break;
    case 'm':			/* menu file name */
	if (menu)
	    return (*quote_func) (menu, 0);
	break;
    case 's':
	if (!panel || !panel->marked)
	    return (*quote_func) (fname, 0);

	/* Fall through */

    case 't':
    case 'u':
	{
	    int length = 2, i;
	    char *block, *tmp;

	    if (!panel)
		return g_strdup ("");

	    for (i = 0; i < panel->count; i++)
		if (panel->dir.list[i].f.marked)
		    length += strlen (panel->dir.list[i].fname) + 1;	/* for space */

	    block = g_malloc (length * 2 + 1);
	    *block = 0;
	    for (i = 0; i < panel->count; i++)
		if (panel->dir.list[i].f.marked) {
		    strcat (block, tmp =
			    (*quote_func) (panel->dir.list[i].fname, 0));
		    g_free (tmp);
		    strcat (block, " ");
		    if (c_lc == 'u')
			do_file_mark (panel, i, 0);
		}
	    return block;
	}			/* sub case block */
    }				/* switch */
    result = g_strdup ("% ");
    result[1] = c;
    return result;
}

/*
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

/* Copies a whitespace separated argument from p to arg. Returns the
   point after argument. */
static char *extract_arg (char *p, char *arg, int size)
{
    char *np;

    while (*p && (*p == ' ' || *p == '\t' || *p == '\n'))
	p++;
                /* support quote space .mnu */
    while (*p && (*p != ' ' || *(p-1) == '\\') && *p != '\t' && *p != '\n') {
	np = str_get_next_char (p);
	if (np - p >= size) break;
	memcpy (arg, p, np - p);
	arg+= np - p;
	size-= np - p;
	p = np;
    }
    *arg = 0;
    if (!*p || *p == '\n')
	str_prev_char (&p);
    return p;
}

/* Tests whether the selected file in the panel is of any of the types
   specified in argument. */
static int test_type (WPanel *panel, char *arg)
{
    int result = 0; /* False by default */
    int st_mode = panel->dir.list [panel->selected].st.st_mode;

    for (;*arg != 0; arg++){
	switch (*arg){
	case 'n':	/* Not a directory */
	    result |= !S_ISDIR (st_mode);
	    break;
	case 'r':	/* Regular file */
	    result |= S_ISREG (st_mode);
	    break;
	case 'd':	/* Directory */
	    result |= S_ISDIR (st_mode);
	    break;
	case 'l':	/* Link */
	    result |= S_ISLNK (st_mode);
	    break;
	case 'c':	/* Character special */
	    result |= S_ISCHR (st_mode);
	    break;
	case 'b':	/* Block special */
	    result |= S_ISBLK (st_mode);
	    break;
	case 'f':	/* Fifo (named pipe) */
	    result |= S_ISFIFO (st_mode);
	    break;
	case 's':	/* Socket */
	    result |= S_ISSOCK (st_mode);
	    break;
	case 'x':	/* Executable */
	    result |= (st_mode & 0111) ? 1 : 0;
	    break;
	case 't':
	    result |= panel->marked ? 1 : 0;
	    break;
	default:
	    debug_error = 1;
	    break;
	}
    }
    return result;
}

/* Calculates the truth value of the next condition starting from
   p. Returns the point after condition. */
static char *
test_condition (WEdit *edit_widget, char *p, int *condition)
{
    WPanel *panel;
    char arg [256];
    mc_search_type_t search_type;

    if (easy_patterns) {
	search_type = MC_SEARCH_T_GLOB;
    } else {
	search_type = MC_SEARCH_T_REGEX;
    }

    /* Handle one condition */
    for (;*p != '\n' && *p != '&' && *p != '|'; p++){
                /* support quote space .mnu */
	if ((*p == ' ' && *(p-1) != '\\') || *p == '\t')
	    continue;
	if (*p >= 'a')
	    panel = current_panel;
	else {
	    if (get_other_type () == view_listing)
		panel = other_panel;
	    else
		panel = NULL;
	}
	*p |= 0x20;

	switch (*p++){
	case '!':
	    p = test_condition (edit_widget, p, condition);
	    *condition = ! *condition;
	    str_prev_char (&p);
	    break;
	case 'f': /* file name pattern */
	    p = extract_arg (p, arg, sizeof (arg));
	    *condition = panel && mc_search (arg, panel->dir.list [panel->selected].fname, search_type);
	    break;
	case 'y': /* syntax pattern */
#ifdef USE_INTERNAL_EDIT
	    if (edit_widget) {
		const char *syntax_type = edit_get_syntax_type (edit_widget);
		if (syntax_type != NULL) {
		    p = extract_arg (p, arg, sizeof (arg));
		    *condition = panel && mc_search (arg, syntax_type, MC_SEARCH_T_NORMAL);
		}
	    }
#endif
            break;
	case 'd':
	    p = extract_arg (p, arg, sizeof (arg));
	    *condition = panel && mc_search (arg, panel->cwd, search_type);
	    break;
	case 't':
	    p = extract_arg (p, arg, sizeof (arg));
	    *condition = panel && test_type (panel, arg);
	    break;
	case 'x': /* executable */
	{
	    struct stat status;

	    p = extract_arg (p, arg, sizeof (arg));
	    if (stat (arg, &status) == 0)
		*condition = is_exe (status.st_mode);
	    else
		*condition = 0; 
	    break;
	}
	default:
	    debug_error = 1;
	    break;
	} /* switch */

    } /* while */
    return p;
}

/* General purpose condition debug output handler */
static void
debug_out (char *start, char *end, int cond)
{
    static char *msg;
    int len;

    if (start == NULL && end == NULL){
	/* Show output */
	if (debug_flag && msg) {
	    len = strlen (msg);
	    if (len)
		msg [len - 1] = 0;
	    message (D_NORMAL, _(" Debug "), "%s", msg);

	}
	debug_flag = 0;
	g_free (msg);
	msg = NULL;
    } else {
	const char *type;
	char *p;

	/* Save debug info for later output */
	if (!debug_flag)
	    return;
	/* Save the result of the condition */
	if (debug_error){
	    type = _(" ERROR: ");
	    debug_error = 0;
	}
	else if (cond)
	    type = _(" True:  ");
	else
	    type = _(" False: ");
	/* This is for debugging, don't need to be super efficient.  */
	if (end == NULL)
	    p = g_strdup_printf ("%s%s%c \n", msg ? msg : "", type, *start);
	else
	    p = g_strdup_printf ("%s%s%.*s \n", msg ? msg : "", type,
			(int) (end - start), start);
	g_free (msg);
	msg = p;
    }
}

/* Calculates the truth value of one lineful of conditions. Returns
   the point just before the end of line. */
static char *
test_line (WEdit *edit_widget, char *p, int *result)
{
    int condition;
    char operator;
    char *debug_start, *debug_end;

    /* Repeat till end of line */
    while (*p && *p != '\n') {
        /* support quote space .mnu */
	while ((*p == ' ' && *(p-1) != '\\' ) || *p == '\t')
	    p++;
	if (!*p || *p == '\n')
	    break;
	operator = *p++;
	if (*p == '?'){
	    debug_flag = 1;
	    p++;
	}
        /* support quote space .mnu */
	while ((*p == ' ' && *(p-1) != '\\' ) || *p == '\t')
	    p++;
	if (!*p || *p == '\n')
	    break;
	condition = 1;	/* True by default */

	debug_start = p;
	p = test_condition (edit_widget, p, &condition);
	debug_end = p;
	/* Add one debug statement */
	debug_out (debug_start, debug_end, condition);

	switch (operator){
	case '+':
	case '=':
	    /* Assignment */
	    *result = condition;
	    break;
	case '&':	/* Logical and */
	    *result &= condition;
	    break;
	case '|':	/* Logical or */
	    *result |= condition;
	    break;
	default:
	    debug_error = 1;
	    break;
	} /* switch */
	/* Add one debug statement */
	debug_out (&operator, NULL, *result);
	
    } /* while (*p != '\n') */
    /* Report debug message */
    debug_out (NULL, NULL, 1);

    if (!*p || *p == '\n')
	str_prev_char (&p);
    return p;
}

/* FIXME: recode this routine on version 3.0, it could be cleaner */
static void
execute_menu_command (WEdit *edit_widget, const char *commands)
{
    FILE *cmd_file;
    int  cmd_file_fd;
    int  expand_prefix_found = 0;
    char *parameter = 0;
    gboolean do_quote = FALSE;
    char lc_prompt [80];
    int  col;
    char *file_name;
    int run_view = 0;

    /* Skip menu entry title line */
    commands = strchr (commands, '\n');
    if (!commands){
	return;
    }

    cmd_file_fd = mc_mkstemps (&file_name, "mcusr", SCRIPT_SUFFIX);

    if (cmd_file_fd == -1){
	message (D_ERROR, MSG_ERROR, _(" Cannot create temporary command file \n %s "),
		 unix_error_string (errno));
	return;
    }
    cmd_file = fdopen (cmd_file_fd, "w");
    fputs ("#! /bin/sh\n", cmd_file);
    commands++;
    
    for (col = 0; *commands; commands++){
	if (col == 0) {
	    if (*commands != ' ' && *commands != '\t')
		break;
	    while (*commands == ' ' || *commands == '\t')
	        commands++;
	    if (*commands == 0)
		break;
	}
	col++;
	if (*commands == '\n')
	    col = 0;
	if (parameter){
	    if (*commands == '}'){
		char *tmp;
		*parameter = 0;
		parameter = input_dialog (_(" Parameter "), lc_prompt, MC_HISTORY_FM_MENU_EXEC_PARAM, "");
		if (!parameter || !*parameter){
		    /* User canceled */
		    fclose (cmd_file);
		    unlink (file_name);
                    g_free (file_name);
		    return;
		}
		if (do_quote) {
		    tmp = name_quote (parameter, 0);
		    fputs (tmp, cmd_file);
		    g_free (tmp);
		} else
		    fputs (parameter, cmd_file);
		g_free (parameter);
		parameter = 0;
	    } else {
		if (parameter < &lc_prompt [sizeof (lc_prompt) - 1]) {
		    *parameter++ = *commands;
		} 
	    }
	} else if (expand_prefix_found){
	    expand_prefix_found = 0;
	    if (g_ascii_isdigit ((gchar) *commands)) {
		do_quote = (atoi (commands) != 0);
		while (g_ascii_isdigit ((gchar) *commands))
		    commands++;
	    }
	    if (*commands == '{')
		parameter = lc_prompt;
	    else{
		char *text = expand_format (edit_widget, *commands, do_quote);
		fputs (text, cmd_file);
		g_free (text);
	    }
	} else {
	    if (*commands == '%') {
		int i = check_format_view (commands + 1);
		if (i) {
		    commands += i;
		    run_view = 1;
		} else {
		    do_quote = TRUE; /* Default: Quote expanded macro */
		    expand_prefix_found = 1;
		}
	    } else
		fputc (*commands, cmd_file);
	}
    }
    fclose (cmd_file);
    chmod (file_name, S_IRWXU);
    if (run_view) {
	run_view = 0;
	mcview_viewer (file_name, NULL, &run_view, 0);
    } else {
	/* execute the command indirectly to allow execution even
	 * on no-exec filesystems. */
	char *cmd = g_strconcat("/bin/sh ", file_name, (char *) NULL);
	shell_execute (cmd, EXECUTE_HIDE);
	g_free(cmd);
    }
    unlink (file_name);
    g_free (file_name);
}

/* 
**	Check owner of the menu file. Using menu file is allowed, if
**	owner of the menu is root or the actual user. In either case
**	file should not be group and word-writable.
**	
**	Q. Should we apply this routine to system and home menu (and .ext files)?
*/
static int
menu_file_own(char* path)
{
    struct stat st;

    if (stat (path, &st) == 0
	&& (!st.st_uid || (st.st_uid == geteuid ()))
	&& ((st.st_mode & (S_IWGRP | S_IWOTH)) == 0)
    ) {
	return 1;
    }
    if (verbose)
    {
	message (D_NORMAL, _(" Warning -- ignoring file "),
		    _("File %s is not owned by root or you or is world writable.\n"
		    "Using it may compromise your security"),
		path
	);
    }
    return 0;
}

/*
 * If edit_widget is NULL then we are called from the mc menu,
 * otherwise we are called from the mcedit menu.
 */
void
user_menu_cmd (struct WEdit *edit_widget)
{
    char *p;
    char *data, **entries;
    int  max_cols, menu_lines, menu_limit;
    int  col, i, accept_entry = 1;
    int  selected, old_patterns;
    Listbox *listbox;
    
    if (!vfs_current_is_local ()){
	message (D_ERROR, MSG_ERROR,
		 _(" Cannot execute commands on non-local filesystems"));
	return;
    }

    menu = g_strdup (edit_widget ? EDIT_LOCAL_MENU : MC_LOCAL_MENU);
    if (!exist_file (menu) || !menu_file_own (menu)){
	g_free (menu);
	if (edit_widget)
	    menu = concat_dir_and_file (home_dir, EDIT_HOME_MENU);
	else
	    menu = g_build_filename (home_dir, MC_USERCONF_DIR, MC_USERMENU_FILE, NULL);


	if (!exist_file (menu)){
	    g_free (menu);
	    menu = concat_dir_and_file
                        (mc_home, edit_widget ? EDIT_GLOBAL_MENU : MC_GLOBAL_MENU);
	    if (!exist_file (menu)) {
		g_free (menu);
		menu = concat_dir_and_file
			(mc_home_alt, edit_widget ? EDIT_GLOBAL_MENU : MC_GLOBAL_MENU);
	    }
	}
    }

    if ((data = load_file (menu)) == NULL){
	message (D_ERROR, MSG_ERROR, _(" Cannot open file %s \n %s "),
		 menu, unix_error_string (errno));
	g_free (menu);
	menu = NULL;
	return;
    }

    max_cols = 0;
    selected = 0;
    menu_limit = 0;
    entries = 0;

    /* Parse the menu file */
    old_patterns = easy_patterns;
    p = check_patterns (data);
    for (menu_lines = col = 0; *p; str_next_char (&p)){
	if (menu_lines >= menu_limit){
	    char ** new_entries;

	    menu_limit += MAX_ENTRIES;
	    new_entries = g_try_realloc (entries, sizeof (new_entries[0]) * menu_limit);

	    if (new_entries == NULL)
		break;

	    entries = new_entries;
	    new_entries += menu_limit;
	    while (--new_entries >= &entries[menu_lines])
		*new_entries = NULL;
	}
	if (col == 0 && !entries [menu_lines]){
	    if (*p == '#'){
		/* A commented menu entry */
		accept_entry = 1;
	    } else if (*p == '+'){
		if (*(p+1) == '='){
		    /* Combined adding and default */
		    p = test_line (edit_widget, p + 1, &accept_entry);
		    if (selected == 0 && accept_entry)
			selected = menu_lines;
		} else {
		    /* A condition for adding the entry */
		    p = test_line (edit_widget, p, &accept_entry);
		}
	    } else if (*p == '='){
		if (*(p+1) == '+'){
		    /* Combined adding and default */
		    p = test_line (edit_widget, p + 1, &accept_entry);
		    if (selected == 0 && accept_entry)
			selected = menu_lines;
		} else {
		    /* A condition for making the entry default */
		    i = 1;
		    p = test_line (edit_widget, p, &i);
		    if (selected == 0 && i)
			selected = menu_lines;
		}
	    }
	    else if (*p != ' ' && *p != '\t' && str_isprint (p)) {
		/* A menu entry title line */
		if (accept_entry)
		    entries [menu_lines] = p;
		else
		    accept_entry = 1;
	    }
	}
	if (*p == '\n'){
	    if (entries [menu_lines]){
		menu_lines++;
		accept_entry = 1;
	    }
	    max_cols = max (max_cols, col);
	    col = 0;
	} else {
	    if (*p == '\t')
		*p = ' ';
	    col++;
	}
    }

    if (menu_lines == 0)
	message (D_ERROR, MSG_ERROR, _(" No suitable entries found in %s "), menu);
    else {
	max_cols = min (max (max_cols, col), MAX_ENTRY_LEN);

	/* Create listbox */
	listbox = create_listbox_window (menu_lines, max_cols + 2,_(" User menu "),
					    "[Menu File Edit]");
	/* insert all the items found */
	for (i = 0; i < menu_lines; i++) {
	    p = entries [i];
	    LISTBOX_APPEND_TEXT (listbox, (unsigned char) p[0],
				extract_line (p, p + MAX_ENTRY_LEN), p);
	}
	/* Select the default entry */
	listbox_select_by_number (listbox->list, selected);

	selected = run_listbox (listbox);
	if (selected >= 0)
	    execute_menu_command (edit_widget, entries [selected]);

	do_refresh ();
    }

    easy_patterns = old_patterns;
    g_free (menu);
    menu = NULL;
    g_free (entries);
    g_free (data);
}
