/* editor syntax highlighting.

   Copyright (C) 1996, 1997, 1998 the Free Software Foundation

   Authors: 1998 Paul Sheer

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.
*/

#include <config.h>
#if defined(MIDNIGHT) || defined(GTK)
#include "edit.h"
#else
#include "coolwidget.h"
#endif

/* bytes */
#define SYNTAX_MARKER_DENSITY 512

#if !defined(MIDNIGHT) || defined(HAVE_SYNTAXH)

int option_syntax_highlighting = 1;

/* these three functions are called from the outside */
void edit_load_syntax (WEdit * edit, char **names, char *type);
void edit_free_syntax_rules (WEdit * edit);
void edit_get_syntax_color (WEdit * edit, long byte_index, int *fg, int *bg);

static void *syntax_malloc (size_t x)
{
    void *p;
    p = malloc (x);
    memset (p, 0, x);
    return p;
}

#define syntax_free(x) {if(x){free(x);(x)=0;}}

static int compare_word_to_right (WEdit * edit, long i, char *text, char *whole_left, char *whole_right, int line_start)
{
    char *p;
    int c, d, j;
    if (!*text)
	return 0;
    c = edit_get_byte (edit, i - 1);
    if (line_start)
	if (c != '\n')
	    return 0;
    if (whole_left)
	if (strchr (whole_left, c))
	    return 0;
    for (p = text; *p; p++, i++) {
	switch (*p) {
	case '\001':
	    p++;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == *p)
		    break;
		if (c == '\n')
		    return 0;
		i++;
	    }
	    break;
	case '\002':
	    p++;
	    j = 0;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == *p)
		    j = i;
		if (j && strchr (p + 1, c))		/* c exists further down, so it will get matched later */
		    break;
		if (c == '\n' || c == '\t' || c == ' ') {
		    if (!j)
			return 0;
		    i = j;
		    break;
		}
		if (whole_right)
		    if (!strchr (whole_right, c)) {
			if (!j)
			    return 0;
			i = j;
			break;
		    }
		i++;
	    }
	    break;
	case '\003':
	    p++;
#if 0
	    c = edit_get_byte (edit, i++);
	    for (j = 0; p[j] != '\003'; j++)
		if (c == p[j])
		    goto found_char1;
	    return 0;
	  found_char1:
#endif
	    c = -1;
	    for (;; i++) {
		d = c;
		c = edit_get_byte (edit, i);
		for (j = 0; p[j] != '\003'; j++)
		    if (c == p[j])
			goto found_char2;
		break;
	      found_char2:
		j = c;		/* dummy command */
	    }
	    i--;
	    while (*p != '\003')
		p++;
	    if (p[1] == d)
		i--;
	    break;
#if 0
	case '\004':
	    p++;
	    c = edit_get_byte (edit, i++);
	    for (j = 0; p[j] != '\004'; j++)
		if (c == p[j])
		    return 0;
	    for (;; i++) {
		c = edit_get_byte (edit, i);
		for (j = 0; p[j] != '\004'; j++)
		    if (c == p[j])
			goto found_char4;
		continue;
	      found_char4:
		break;
	    }
	    i--;
	    while (*p != '\004')
		p++;
	    break;
#endif
	default:
	    if (*p != edit_get_byte (edit, i))
		return 0;
	}
    }
    if (whole_right)
	if (strchr (whole_right, edit_get_byte (edit, i)))
	    return 0;
    return 1;
}

static int compare_word_to_left (WEdit * edit, long i, char *text, char *whole_left, char *whole_right, int line_start)
{
    char *p;
    int c, d, j;
    if (!*text)
	return 0;
    if (whole_right)
	if (strchr (whole_right, edit_get_byte (edit, i + 1)))
	    return 0;
    for (p = text + strlen (text) - 1; (unsigned long) p >= (unsigned long) text; p--, i--) {
	switch (*p) {
	case '\001':
	    p--;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == *p)
		    break;
		if (c == '\n')
		    return 0;
		i--;
	    }
	    break;
	case '\002':
	    p--;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == *p)
		    break;
		if (c == '\n' || c == '\t' || c == ' ')
		    return 0;
		if (whole_right)
		    if (!strchr (whole_right, c))
			return 0;
		i--;
	    }
	    break;
	case '\003':
	    while (*(--p) != '\003');
	    p++;
#if 0
	    c = edit_get_byte (edit, i--);
	    for (j = 0; p[j] != '\003'; j++)
		if (c == p[j])
		    goto found_char1;
	    return 0;
	  found_char1:
#endif
	    c = -1;
	    d = '\0';
	    for (;; i--) {
		d = c;
		c = edit_get_byte (edit, i);
		for (j = 0; p[j] != '\003'; j++)
		    if (c == p[j])
			goto found_char2;
		break;
	      found_char2:
		j = c;	/* dummy command */
	    }
	    i++;
	    p--;
	    if (*(p - 1) == d)
		i++;
	    break;
#if 0
	case '\004':
	    while (*(--p) != '\004');
	    d = *p;
	    p++;
	    c = edit_get_byte (edit, i--);
	    for (j = 0; p[j] != '\004'; j++)
		if (c == p[j])
		    return 0;
	    for (;; i--) {
		c = edit_get_byte (edit, i);
		for (j = 0; p[j] != '\004'; j++)
		    if (c == p[j] || c == '\n' || c == d)
			goto found_char4;
		continue;
	      found_char4:
		break;
	    }
	    i++;
	    p--;
	    break;
#endif
	default:
	    if (*p != edit_get_byte (edit, i))
		return 0;
	}
    }
    c = edit_get_byte (edit, i);
    if (line_start)
	if (c != '\n')
	    return 0;
    if (whole_left)
	if (strchr (whole_left, c))
	    return 0;
    return 1;
}


#if 0
#define debug_printf(x,y) fprintf(stderr,x,y)
#else
#define debug_printf(x,y)
#endif

static inline unsigned long apply_rules_going_right (WEdit * edit, long i, unsigned long rule)
{
    struct context_rule *r;
    int context, contextchanged = 0, keyword, c1, c2;
    int found_right = 0, found_left = 0, keyword_foundleft = 0;
    int done = 0;
    unsigned long border;
    context = (rule & RULE_CONTEXT) >> RULE_CONTEXT_SHIFT;
    keyword = (rule & RULE_WORD) >> RULE_WORD_SHIFT;
    border = rule & (RULE_ON_LEFT_BORDER | RULE_ON_RIGHT_BORDER);
    c1 = edit_get_byte (edit, i - 1);
    c2 = edit_get_byte (edit, i);
    if (!c2 || !c1)
	return rule;

    debug_printf ("%c->", c1);
    debug_printf ("%c ", c2);

/* check to turn off a keyword */
    if (keyword) {
	struct key_word *k;
	k = edit->rules[context]->keyword[keyword];
	if (c1 == '\n')
	    keyword = 0;
	if (k->last == c1 && compare_word_to_left (edit, i - 1, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
	    keyword = 0;
	    keyword_foundleft = 1;
	    debug_printf ("keyword=%d ", keyword);
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");

/* check to turn off a context */
    if (context && !keyword) {
	r = edit->rules[context];
	if (r->first_right == c2 && compare_word_to_right (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right) \
	    &&!(rule & RULE_ON_RIGHT_BORDER)) {
	    debug_printf ("A:3 ", 0);
	    found_right = 1;
	    border = RULE_ON_RIGHT_BORDER;
	    if (r->between_delimiters)
		context = 0;
	} else if (!found_left) {
	    if (r->last_right == c1 && compare_word_to_left (edit, i - 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right) \
		&&(rule & RULE_ON_RIGHT_BORDER)) {
/* always turn off a context at 4 */
		debug_printf ("A:4 ", 0);
		found_left = 1;
		border = 0;
		if (!keyword_foundleft)
		    context = 0;
	    } else if (r->last_left == c1 && compare_word_to_left (edit, i - 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left) \
		       &&(rule & RULE_ON_LEFT_BORDER)) {
/* never turn off a context at 2 */
		debug_printf ("A:2 ", 0);
		found_left = 1;
		border = 0;
	    }
	}
    }
    debug_printf ("\n", 0);

/* check to turn on a keyword */
    if (!keyword) {
	char *p;
	p = (r = edit->rules[context])->keyword_first_chars;
	while ((p = strchr (p + 1, c2))) {
	    struct key_word *k;
	    int count;
	    count = (unsigned long) p - (unsigned long) r->keyword_first_chars;
	    k = r->keyword[count];
	    if (compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
		keyword = count;
		debug_printf ("keyword=%d ", keyword);
		break;
	    }
	}
    }
/* check to turn on a context */
    if (!context) {
	int count;
	for (count = 1; edit->rules[count] && !done; count++) {
	    r = edit->rules[count];
	    if (!found_left) {
		if (r->last_right == c1 && compare_word_to_left (edit, i - 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right) \
		    &&(rule & RULE_ON_RIGHT_BORDER)) {
		    debug_printf ("B:4 count=%d", count);
		    found_left = 1;
		    border = 0;
		    context = 0;
		    contextchanged = 1;
		    keyword = 0;
		} else if (r->last_left == c1 && compare_word_to_left (edit, i - 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left) \
			   &&(rule & RULE_ON_LEFT_BORDER)) {
		    debug_printf ("B:2 ", 0);
		    found_left = 1;
		    border = 0;
		    if (r->between_delimiters) {
			context = count;
			contextchanged = 1;
			keyword = 0;
			debug_printf ("context=%d ", context);
			if (r->first_right == c2 && compare_word_to_right (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
			    debug_printf ("B:3 ", 0);
			    found_right = 1;
			    border = RULE_ON_RIGHT_BORDER;
			    context = 0;
			}
		    }
		    break;
		}
	    }
	    if (!found_right) {
		if (r->first_left == c2 && compare_word_to_right (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
		    debug_printf ("B:1 ", 0);
		    found_right = 1;
		    border = RULE_ON_LEFT_BORDER;
		    if (!r->between_delimiters) {
			debug_printf ("context=%d ", context);
			if (!keyword)
			    context = count;
		    }
		    break;
		}
	    }
	}
    }
    if (!keyword && contextchanged) {
	char *p;
	p = (r = edit->rules[context])->keyword_first_chars;
	while ((p = strchr (p + 1, c2))) {
	    struct key_word *k;
	    int coutner;
	    coutner = (unsigned long) p - (unsigned long) r->keyword_first_chars;
	    k = r->keyword[coutner];
	    if (compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
		keyword = coutner;
		debug_printf ("keyword=%d ", keyword);
		break;
	    }
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");
    debug_printf ("keyword=%d ", keyword);

    debug_printf (" %d#\n\n", context);

    return (context << RULE_CONTEXT_SHIFT) | (keyword << RULE_WORD_SHIFT) | border;
}

static unsigned long edit_get_rule (WEdit * edit, long byte_index)
{
    long i;
    if (byte_index < 0) {
	edit->last_get_rule = -1;
	edit->rule = 0;
	return 0;
    }
    if (byte_index > edit->last_get_rule) {
	for (i = edit->last_get_rule + 1; i <= byte_index; i++) {
	    edit->rule = apply_rules_going_right (edit, i, edit->rule);
	    if (i > (edit->syntax_marker ? edit->syntax_marker->offset + SYNTAX_MARKER_DENSITY : SYNTAX_MARKER_DENSITY)) {
		struct _syntax_marker *s;
		s = edit->syntax_marker;
		edit->syntax_marker = malloc (sizeof (struct _syntax_marker));
		edit->syntax_marker->next = s;
		edit->syntax_marker->offset = i;
		edit->syntax_marker->rule = edit->rule;
	    }
	}
    } else if (byte_index < edit->last_get_rule) {
	struct _syntax_marker *s;
	for (;;) {
	    if (!edit->syntax_marker) {
		edit->rule = 0;
		for (i = -1; i <= byte_index; i++)
		    edit->rule = apply_rules_going_right (edit, i, edit->rule);
		break;
	    }
	    if (byte_index >= edit->syntax_marker->offset) {
		edit->rule = edit->syntax_marker->rule;
		for (i = edit->syntax_marker->offset + 1; i <= byte_index; i++)
		    edit->rule = apply_rules_going_right (edit, i, edit->rule);
		break;
	    }
	    s = edit->syntax_marker->next;
	    syntax_free (edit->syntax_marker);
	    edit->syntax_marker = s;
	}
    }
    edit->last_get_rule = byte_index;
    return edit->rule;
}

static void translate_rule_to_color (WEdit * edit, unsigned long rule, int *fg, int *bg)
{
    struct key_word *k;
    k = edit->rules[(rule & RULE_CONTEXT) >> RULE_CONTEXT_SHIFT]->keyword[(rule & RULE_WORD) >> RULE_WORD_SHIFT];
    *bg = k->bg;
    *fg = k->fg;
}

void edit_get_syntax_color (WEdit * edit, long byte_index, int *fg, int *bg)
{
    unsigned long rule;
    if (!edit->rules || byte_index >= edit->last_byte || !option_syntax_highlighting) {
#ifdef MIDNIGHT
	*fg = EDITOR_NORMAL_COLOR;
#else
	*fg = NO_COLOR;
	*bg = NO_COLOR;
#endif
    } else {
	rule = edit_get_rule (edit, byte_index);
	translate_rule_to_color (edit, rule, fg, bg);
    }
}


/*
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
 */
static int read_one_line (char **line, FILE * f)
{
    char *p;
    int len = 256, c, r = 0, i = 0;
    p = syntax_malloc (len);
    for (;;) {
	c = fgetc (f);
	if (c == -1) {
	    r = 0;
	    break;
	} else if (c == '\n') {
	    r = i + 1;		/* extra 1 for the newline just read */
	    break;
	} else {
	    if (i >= len - 1) {
		char *q;
		q = syntax_malloc (len * 2);
		memcpy (q, p, len);
		syntax_free (p);
		p = q;
		len *= 2;
	    }
	    p[i++] = c;
	}
    }
    p[i] = 0;
    *line = p;
    return r;
}

static char *strdup_convert (char *s)
{
#if 0
    int e = 0;
#endif
    char *r, *p;
    p = r = strdup (s);
    while (*s) {
	switch (*s) {
	case '\\':
	    s++;
	    switch (*s) {
	    case ' ':
		*p = ' ';
		s--;
		break;
	    case 'n':
		*p = '\n';
		break;
	    case 'r':
		*p = '\r';
		break;
	    case 't':
		*p = '\t';
		break;
	    case 's':
		*p = ' ';
		break;
	    case '*':
		*p = '*';
		break;
	    case '\\':
		*p = '\\';
		break;
	    case '[':
	    case ']':
		if ((unsigned long) p == (unsigned long) r || strlen (s) == 1)
		    *p = *s;
		else {
#if 0
		    if (!strncmp (s, "[^", 2)) {
			*p = '\004';
			e = 1;
			s++;
		    } else {
			if (e)
			    *p = '\004';
			else
#endif
			    *p = '\003';
#if 0
			e = 0;
		    }
#endif
		}
		break;
	    default:
		*p = *s;
		break;
	    }
	    break;
	case '*':
/* a * or + at the beginning or end of the line must be interpreted literally */
	    if ((unsigned long) p == (unsigned long) r || strlen (s) == 1)
		*p = '*';
	    else
		*p = '\001';
	    break;
	case '+':
	    if ((unsigned long) p == (unsigned long) r || strlen (s) == 1)
		*p = '+';
	    else
		*p = '\002';
	    break;
	default:
	    *p = *s;
	    break;
	}
	s++;
	p++;
    }
    *p = 0;
    return r;
}

#define whiteness(x) ((x) == '\t' || (x) == '\n' || (x) == ' ')

static void get_args (char *l, char **args, int *argc)
{
    *argc = 0;
    l--;
    for (;;) {
	char *p;
	for (p = l + 1; *p && whiteness (*p); p++);
	if (!*p)
	    break;
	for (l = p + 1; *l && !whiteness (*l); l++);
	*l = '\0';
	*args = strdup_convert (p);
	(*argc)++;
	args++;
    }
    *args = 0;
}

static void free_args (char **args)
{
    while (*args) {
	syntax_free (*args);
	*args = 0;
	args++;
    }
}

#define check_a {if(!*a){result=line;break;}}
#define check_not_a {if(*a){result=line;break;}}

#ifdef MIDNIGHT

int try_alloc_color_pair (char *fg, char *bg);

int this_try_alloc_color_pair (char *fg, char *bg)
{
    char f[80], b[80], *p;
    if (bg)
	if (!*bg)
	    bg = 0;
    if (fg)
	if (!*fg)
	    fg = 0;
    if (fg) {
	strcpy (f, fg);
	p = strchr (f, '/');
	if (p)
	    *p = '\0';
	fg = f;
    }
    if (bg) {
	strcpy (b, bg);
	p = strchr (b, '/');
	if (p)
	    *p = '\0';
	bg = b;
    }
    return try_alloc_color_pair (fg, bg);
}
#else
#ifdef GTK
int allocate_color (WEdit *edit, gchar *color);

int this_allocate_color (WEdit *edit, char *fg)
{
    char *p;
    if (fg)
	if (!*fg)
	    fg = 0;
    if (!fg)
	return allocate_color (edit, 0);
    p = strchr (fg, '/');
    if (!p)
	return allocate_color (edit, fg);
    return allocate_color (edit, p + 1);
}
#else
int this_allocate_color (WEdit *edit, char *fg)
{
    char *p;
    if (fg)
	if (!*fg)
	    fg = 0;
    if (!fg)
	return allocate_color (0);
    p = strchr (fg, '/');
    if (!p)
	return allocate_color (fg);
    return allocate_color (p + 1);
}
#endif
#endif

static char *error_file_name = 0;

static FILE *open_include_file (char *filename)
{
    FILE *f;
    char p[MAX_PATH_LEN];
    syntax_free (error_file_name);
    error_file_name = strdup (filename);
    if (*filename == '/')
	return fopen (filename, "r");
    strcpy (p, home_dir);
    strcat (p, EDIT_DIR "/");
    strcat (p, filename);
    syntax_free (error_file_name);
    error_file_name = strdup (p);
    f = fopen (p, "r");
    if (f)
	return f;
    strcpy (p, LIBDIR "/syntax/");
    strcat (p, filename);
    syntax_free (error_file_name);
    error_file_name = strdup (p);
    return fopen (p, "r");
}

/* returns line number on error */
static int edit_read_syntax_rules (WEdit * edit, FILE * f)
{
    FILE *g = 0;
    char *fg, *bg;
    char last_fg[32] = "", last_bg[32] = "";
    char whole_right[512];
    char whole_left[512];
    char *args[1024], *l = 0;
    int save_line = 0, line = 0;
    struct context_rule **r, *c;
    int num_words = -1, num_contexts = -1;
    int argc, result = 0;
    int i, j;

    args[0] = 0;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    r = edit->rules = syntax_malloc (256 * sizeof (struct context_rule *));

    for (;;) {
	char **a;
	line++;
	l = 0;
	if (!read_one_line (&l, f)) {
	    if (g) {
		fclose (f);
		f = g;
		g = 0;
		line = save_line + 1;
		syntax_free (error_file_name);
		if (l)
		    free (l);
		if (!read_one_line (&l, f))
		    break;
	    } else {
		break;
	    }
	}
	get_args (l, args, &argc);
	a = args + 1;
	if (!args[0]) {
	    /* do nothing */
	} else if (!strcmp (args[0], "include")) {
	    if (g || argc != 2) {
		result = line;
		break;
	    }
	    g = f;
	    f = open_include_file (args[1]);
	    if (!f) {
		syntax_free (error_file_name);
		result = line;
		break;
	    }
	    save_line = line;
	    line = 0;
	} else if (!strcmp (args[0], "wholechars")) {
	    check_a;
	    if (!strcmp (*a, "left")) {
		a++;
		strcpy (whole_left, *a);
	    } else if (!strcmp (*a, "right")) {
		a++;
		strcpy (whole_right, *a);
	    } else {
		strcpy (whole_left, *a);
		strcpy (whole_right, *a);
	    }
	    a++;
	    check_not_a;
	} else if (!strcmp (args[0], "context")) {
	    check_a;
	    if (num_contexts == -1) {
		if (strcmp (*a, "default")) {	/* first context is the default */
		    *a = 0;
		    check_a;
		}
		a++;
		c = r[0] = syntax_malloc (sizeof (struct context_rule));
		c->left = strdup (" ");
		c->right = strdup (" ");
		num_contexts = 0;
	    } else {
		c = r[num_contexts] = syntax_malloc (sizeof (struct context_rule));
		if (!strcmp (*a, "exclusive")) {
		    a++;
		    c->between_delimiters = 1;
		}
		check_a;
		if (!strcmp (*a, "whole")) {
		    a++;
		    c->whole_word_chars_left = strdup (whole_left);
		    c->whole_word_chars_right = strdup (whole_right);
		} else if (!strcmp (*a, "wholeleft")) {
		    a++;
		    c->whole_word_chars_left = strdup (whole_left);
		} else if (!strcmp (*a, "wholeright")) {
		    a++;
		    c->whole_word_chars_right = strdup (whole_right);
		}
		check_a;
		if (!strcmp (*a, "linestart")) {
		    a++;
		    c->line_start_left = 1;
		}
		check_a;
		c->left = strdup (*a++);
		check_a;
		if (!strcmp (*a, "linestart")) {
		    a++;
		    c->line_start_right = 1;
		}
		check_a;
		c->right = strdup (*a++);
		c->last_left = c->left[strlen (c->left) - 1];
		c->last_right = c->right[strlen (c->right) - 1];
		c->first_left = *c->left;
		c->first_right = *c->right;
		c->single_char = (strlen (c->right) == 1);
	    }
	    c->keyword = syntax_malloc (1024 * sizeof (struct key_word *));
	    num_words = 1;
	    c->keyword[0] = syntax_malloc (sizeof (struct key_word));
	    fg = *a;
	    if (*a)
		a++;
	    bg = *a;
	    if (*a)
		a++;
	    strcpy (last_fg, fg ? fg : "");
	    strcpy (last_bg, bg ? bg : "");
#ifdef MIDNIGHT
	    c->keyword[0]->fg = this_try_alloc_color_pair (fg, bg);
#else
	    c->keyword[0]->fg = this_allocate_color (edit, fg);
	    c->keyword[0]->bg = this_allocate_color (edit, bg);
#endif
	    c->keyword[0]->keyword = strdup (" ");
	    check_not_a;
	    num_contexts++;
	} else if (!strcmp (args[0], "keyword")) {
	    struct key_word *k;
	    if (num_words == -1)
		*a = 0;
	    check_a;
	    k = r[num_contexts - 1]->keyword[num_words] = syntax_malloc (sizeof (struct key_word));
	    if (!strcmp (*a, "whole")) {
		a++;
		k->whole_word_chars_left = strdup (whole_left);
		k->whole_word_chars_right = strdup (whole_right);
	    } else if (!strcmp (*a, "wholeleft")) {
		a++;
		k->whole_word_chars_left = strdup (whole_left);
	    } else if (!strcmp (*a, "wholeright")) {
		a++;
		k->whole_word_chars_right = strdup (whole_right);
	    }
	    check_a;
	    if (!strcmp (*a, "linestart")) {
		a++;
		k->line_start = 1;
	    }
	    check_a;
	    if (!strcmp (*a, "whole")) {
		*a = 0;
		check_a;
	    }
	    k->keyword = strdup (*a++);
	    k->last = k->keyword[strlen (k->keyword) - 1];
	    k->first = *k->keyword;
	    fg = *a;
	    if (*a)
		a++;
	    bg = *a;
	    if (*a)
		a++;
	    if (!fg)
		fg = last_fg;
	    if (!bg)
		bg = last_bg;
#ifdef MIDNIGHT
	    k->fg = this_try_alloc_color_pair (fg, bg);
#else
	    k->fg = this_allocate_color (edit, fg);
	    k->bg = this_allocate_color (edit, bg);
#endif
	    check_not_a;
	    num_words++;
	} else if (!strncmp (args[0], "#", 1)) {
	    /* do nothing for comment */
	} else if (!strcmp (args[0], "file")) {
	    break;
	} else {		/* anything else is an error */
	    *a = 0;
	    check_a;
	}
	free_args (args);
	syntax_free (l);
    }
    free_args (args);
    syntax_free (l);

    if (result)
	return result;

    if (num_contexts == -1) {
	result = line;
	return result;
    }

    {
	char first_chars[1024], *p;
	char last_chars[1024], *q;
	for (i = 0; edit->rules[i]; i++) {
	    c = edit->rules[i];
	    p = first_chars;
	    q = last_chars;
	    *p++ = (char) 1;
	    *q++ = (char) 1;
	    for (j = 1; c->keyword[j]; j++) {
		*p++ = c->keyword[j]->first;
		*q++ = c->keyword[j]->last;
	    }
	    *p = '\0';
	    *q = '\0';
	    c->keyword_first_chars = strdup (first_chars);
	    c->keyword_last_chars = strdup (last_chars);
	}
    }

    return result;
}

void (*syntax_change_callback) (CWidget *) = 0;

void edit_set_syntax_change_callback (void (*callback) (CWidget *))
{
    syntax_change_callback = callback;
}

void edit_free_syntax_rules (WEdit * edit)
{
    int i, j;
    if (!edit)
	return;
    if (!edit->rules)
	return;
    syntax_free (edit->syntax_type);
    edit->syntax_type = 0;
    if (syntax_change_callback)
#ifdef MIDNIGHT
	(*syntax_change_callback) (&edit->widget);
#else
	(*syntax_change_callback) (edit->widget);
#endif
    for (i = 0; edit->rules[i]; i++) {
	if (edit->rules[i]->keyword) {
	    for (j = 0; edit->rules[i]->keyword[j]; j++) {
		syntax_free (edit->rules[i]->keyword[j]->keyword);
		syntax_free (edit->rules[i]->keyword[j]->whole_word_chars_left);
		syntax_free (edit->rules[i]->keyword[j]->whole_word_chars_right);
		syntax_free (edit->rules[i]->keyword[j]);
	    }
	}
	syntax_free (edit->rules[i]->left);
	syntax_free (edit->rules[i]->right);
	syntax_free (edit->rules[i]->whole_word_chars_left);
	syntax_free (edit->rules[i]->whole_word_chars_right);
	syntax_free (edit->rules[i]->keyword);
	syntax_free (edit->rules[i]->keyword_first_chars);
	syntax_free (edit->rules[i]->keyword_last_chars);
	syntax_free (edit->rules[i]);
    }
    for (;;) {
	struct _syntax_marker *s;
	if (!edit->syntax_marker)
	    break;
	s = edit->syntax_marker->next;
	syntax_free (edit->syntax_marker);
	edit->syntax_marker = s;
    }
    syntax_free (edit->rules);
}

#define CURRENT_SYNTAX_RULES_VERSION "48"

char *syntax_text[] = {
"# syntax rules version " CURRENT_SYNTAX_RULES_VERSION,
"# (after the slash is a Cooledit color, 0-26 or any of the X colors in rgb.txt)",
"# black",
"# red",
"# green",
"# brown",
"# blue",
"# magenta",
"# cyan",
"# lightgray",
"# gray",
"# brightred",
"# brightgreen",
"# yellow",
"# brightblue",
"# brightmagenta",
"# brightcyan",
"# white",
"",
"file gobledy_gook #\\sHelp\\ssupport\\sother\\sfile\\stypes",
"context default",
"file gobledy_gook #\\sby\\scoding\\srules\\sin\\s~/.cedit/syntax.",
"context default",
"file gobledy_gook #\\sSee\\sman/syntax\\sin\\sthe\\ssource\\sdistribution",
"context default",
"file gobledy_gook #\\sand\\sconsult\\sthe\\sman\\spage.",
"context default",
"",
"",
"file ..\\*\\\\.diff$ Unified\\sDiff\\sOutput ^diff.\\*(-u|--unified)",
"include diff.syntax",
"",
"file ..\\*\\\\.diff$ Context\\sDiff\\sOutput ^diff.\\*(-c|--context)",
"include diffc.syntax",
"",
"file ..\\*\\\\.lsm$ LSM\\sFile",
"include lsm.syntax",
"",
"file ..\\*\\\\.sh$ Shell\\sScript ^#!\\s\\*/.\\*/(ksh|bash|sh|pdkzsh)$",
"include sh.syntax",
"",
"file ..\\*\\\\.(pl|PL])$ Perl\\sProgram ^#!\\s\\*/.\\*/perl$",
"include perl.syntax",
"",
"file ..\\*\\\\.(py|PY])$ Python\\sProgram ^#!\\s\\*/.\\*/python$",
"include python.syntax",
"",
"file ..\\*\\\\.(man|[0-9n]|[0-9]x)$ NROFF\\sSource",
"include nroff.syntax",
"",
"file ..\\*\\\\.(htm|html|HTM|HTML)$ HTML\\sFile",
"include html.syntax",
"",
"file ..\\*\\\\.(pp|PP|pas|PAS|)$ Pascal Program",
"include pascal.syntax",
"",
"file ..\\*\\\\.tex$ LaTeX\\s2.09\\sDocument",
"include latex.syntax",
"",
"file ..\\*\\\\.([chC]|CC|cxx|cc|cpp|CPP|CXX)$ C/C\\+\\+\\sProgram",
"include c.syntax",
"",
"file ..\\*\\\\.(java|JAVA|Java|jav)$ Java\\sProgram",
"include java.syntax",
"",
"file ..\\*\\\\.(st)$ SmallTalk\\sProgram",
"include smalltalk.syntax",
"",
"file ..\\*\\\\.(ml|mli|mly|mll|mlp)$ ML\\sProgram",
"include ml.syntax",
"",
"file .\\*ChangeLog$ GNU\\sDistribution\\sChangeLog\\sFile",
"include changelog.syntax",
"",
"file .\\*Makefile[\\\\\\.a-z]\\*$ Makefile",
"include makefile.syntax",
"",
"file .\\*syntax$ Syntax\\sHighlighting\\sdefinitions",
"",
"context default",
"    keyword whole keyw\\ord yellow/24",
"    keyword whole whole\\[\\t\\s\\]l\\inestart brightcyan/17",
"    keyword whole whole\\[\\t\\s\\]l\\inestart brightcyan/17",
"    keyword whole wh\\oleleft\\[\\t\\s\\]l\\inestart brightcyan/17",
"    keyword whole wh\\oleright\\[\\t\\s\\]l\\inestart brightcyan/17",
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\ole",
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\ole",
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\oleleft",
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\oleright",
"    keyword wholeleft whole\\s brightcyan/17",
"    keyword wholeleft whole\\t brightcyan/17",
"    keyword whole wh\\oleleft brightcyan/17",
"    keyword whole wh\\oleright brightcyan/17",
"    keyword whole lin\\[e\\]start brightcyan/17",
"    keyword whole c\\ontext\\[\\t\\s\\]exclusive brightred/18",
"    keyword whole c\\ontext\\[\\t\\s\\]default brightred/18",
"    keyword whole c\\ontext brightred/18",
"    keyword whole wh\\olechars\\[\\t\\s\\]left brightcyan/17",
"    keyword whole wh\\olechars\\[\\t\\s\\]right brightcyan/17",
"    keyword whole wh\\olechars brightcyan/17",
"    keyword whole f\\ile brightgreen/6",
"    keyword whole in\\clude brightred/18",
"",
"    keyword whole 0 lightgray/0	blue/26",
"    keyword whole 1 lightgray/1	blue/26",
"    keyword whole 2 lightgray/2	blue/26",
"    keyword whole 3 lightgray/3	blue/26",
"    keyword whole 4 lightgray/4	blue/26",
"    keyword whole 5 lightgray/5	blue/26",
"    keyword whole 6 lightgray/6",
"    keyword whole 7 lightgray/7",
"    keyword whole 8 lightgray/8",
"    keyword whole 9 lightgray/9",
"    keyword whole 10 lightgray/10",
"    keyword whole 11 lightgray/11",
"    keyword whole 12 lightgray/12",
"    keyword whole 13 lightgray/13",
"    keyword whole 14 lightgray/14",
"    keyword whole 15 lightgray/15",
"    keyword whole 16 lightgray/16",
"    keyword whole 17 lightgray/17",
"    keyword whole 18 lightgray/18",
"    keyword whole 19 lightgray/19",
"    keyword whole 20 lightgray/20",
"    keyword whole 21 lightgray/21",
"    keyword whole 22 lightgray/22",
"    keyword whole 23 lightgray/23",
"    keyword whole 24 lightgray/24",
"    keyword whole 25 lightgray/25",
"    keyword whole 26 lightgray/26",
"",
"    keyword wholeleft black\\/ black/0",
"    keyword wholeleft red\\/ red/DarkRed",
"    keyword wholeleft green\\/ green/green3",
"    keyword wholeleft brown\\/ brown/saddlebrown",
"    keyword wholeleft blue\\/ blue/blue3",
"    keyword wholeleft magenta\\/ magenta/magenta3",
"    keyword wholeleft cyan\\/ cyan/cyan3",
"    keyword wholeleft lightgray\\/ lightgray/lightgray",
"    keyword wholeleft gray\\/ gray/gray",
"    keyword wholeleft brightred\\/ brightred/red",
"    keyword wholeleft brightgreen\\/ brightgreen/green1",
"    keyword wholeleft yellow\\/ yellow/yellow",
"    keyword wholeleft brightblue\\/ brightblue/blue1",
"    keyword wholeleft brightmagenta\\/ brightmagenta/magenta",
"    keyword wholeleft brightcyan\\/ brightcyan/cyan1",
"    keyword wholeleft white\\/ white/26",
"",
"context linestart # \\n brown/22",
"",
0};


FILE *upgrade_syntax_file (char *syntax_file)
{
    FILE *f;
    char line[80];
    f = fopen (syntax_file, "r");
    if (!f) {
	char **syntax_line;
	f = fopen (syntax_file, "w");
	if (!f)
	    return 0;
	for (syntax_line = syntax_text; *syntax_line; syntax_line++)
	    fprintf (f, "%s\n", *syntax_line);
	fclose (f);
	return fopen (syntax_file, "r");
    }
    memset (line, 0, 79);
    fread (line, 80, 1, f);
    if (!strstr (line, "syntax rules version")) {
	goto rename_rule_file;
    } else {
	char *p;
	p = strstr (line, "version") + strlen ("version") + 1;
	if (atoi (p) < atoi (CURRENT_SYNTAX_RULES_VERSION)) {
	    char s[1024];
	  rename_rule_file:
	    strcpy (s, syntax_file);
	    strcat (s, ".OLD");
	    unlink (s);
	    rename (syntax_file, s);
	    unlink (syntax_file);	/* might rename() fail ? */
#if defined(MIDNIGHT) || defined(GTK)
	    edit_message_dialog (" Load Syntax Rules ", " Your syntax rule file is outdated \n A new rule file is being installed. \n Your old rule file has been saved with a .OLD extension. ");
#else
	    CMessageDialog (0, 20, 20, 0, " Load Syntax Rules ", " Your syntax rule file is outdated \n A new rule file is being installed. \n Your old rule file has been saved with a .OLD extension. ");
#endif
	    return upgrade_syntax_file (syntax_file);
	} else {
	    rewind (f);
	    return (f);
	}
    }
    return 0;			/* not reached */
}

/* returns -1 on file error, line number on error in file syntax */
static int edit_read_syntax_file (WEdit * edit, char **names, char *syntax_file, char *editor_file, char *first_line, char *type)
{
    FILE *f;
    regex_t r, r2;
    regmatch_t pmatch[1];
    char *args[1024], *l;
    int line = 0;
    int argc;
    int result = 0;
    int count = 0;

    f = upgrade_syntax_file (syntax_file);
    if (!f)
	return -1;
    args[0] = 0;

    for (;;) {
	line++;
	if (!read_one_line (&l, f))
	    break;
	get_args (l, args, &argc);
	if (!args[0]) {
	} else if (!strcmp (args[0], "file")) {
	    if (!args[1] || !args[2]) {
		result = line;
		break;
	    }
	    if (regcomp (&r, args[1], REG_EXTENDED)) {
		result = line;
		break;
	    }
	    if (regcomp (&r2, args[3] ? args[3] : "^nEvEr MaTcH aNyThInG$", REG_EXTENDED)) {
		result = line;
		break;
	    }
	    if (names) {
		names[count++] = strdup (args[2]);
		names[count] = 0;
	    } else if (type) {
		if (!strcmp (type, args[2]))
		    goto found_type;
	    } else if (editor_file && edit) {
		if (!regexec (&r, editor_file, 1, pmatch, 0) || !regexec (&r2, first_line, 1, pmatch, 0)) {
		    int line_error;
		  found_type:
		    line_error = edit_read_syntax_rules (edit, f);
		    if (line_error) {
			if (!error_file_name)	/* an included file */
			    result = line + line_error;
			else
			    result = line_error;
		    } else {
			syntax_free (edit->syntax_type);
			edit->syntax_type = strdup (args[2]);
			if (syntax_change_callback)
#ifdef MIDNIGHT
			    (*syntax_change_callback) (&edit->widget);
#else
			    (*syntax_change_callback) (edit->widget);
#endif
/* if there are no rules then turn off syntax highlighting for speed */
			if (!edit->rules[1])
			    if (!edit->rules[0]->keyword[1])
				edit_free_syntax_rules (edit);
		    }
		    break;
		}
	    }
	}
	free_args (args);
	syntax_free (l);
    }
    free_args (args);
    syntax_free (l);

    fclose (f);

    return result;
}

static char *get_first_editor_line (WEdit * edit)
{
    int i;
    static char s[256];
    s[0] = '\0';
    if (!edit)
	return s;
    for (i = 0; i < 255; i++) {
	s[i] = edit_get_byte (edit, i);
	if (s[i] == '\n') {
	    s[i] = '\0';
	    break;
	}
    }
    s[255] = '\0';
    return s;
}

/* loads rules into edit struct. one of edit or names must be zero. if
   edit is zero, a list of types will be stored into name. type may be zero
   in which case the type will be selected according to the filename. */
void edit_load_syntax (WEdit * edit, char **names, char *type)
{
    int r;
    char *f;

    edit_free_syntax_rules (edit);

#ifdef MIDNIGHT
    if (!SLtt_Use_Ansi_Colors || !use_colors)
	return;
#endif

    if (edit) {
	if (!edit->filename)
	    return;
	if (!*edit->filename && !type)
	    return;
    }
    f = catstrs (home_dir, SYNTAX_FILE, 0);
    r = edit_read_syntax_file (edit, names, f, edit ? edit->filename : 0, get_first_editor_line (edit), type);
    if (r == -1) {
	edit_free_syntax_rules (edit);
	edit_error_dialog (_ (" Load syntax file "), _ (" File access error "));
	return;
    }
    if (r) {
	char s[80];
	edit_free_syntax_rules (edit);
	sprintf (s, _ (" Error in file %s on line %d "), error_file_name ? error_file_name : f, r);
	edit_error_dialog (_ (" Load syntax file "), s);
	syntax_free (error_file_name);
	return;
    }
}

#else

int option_syntax_highlighting = 0;

void edit_load_syntax (WEdit * edit, char **names, char *type)
{
    return;
}

void edit_free_syntax_rules (WEdit * edit)
{
    return;
}

void edit_get_syntax_color (WEdit * edit, long byte_index, int *fg, int *bg)
{
    *fg = NORMAL_COLOR;
}

#endif		/* !defined(MIDNIGHT) || defined(HAVE_SYNTAXH) */



