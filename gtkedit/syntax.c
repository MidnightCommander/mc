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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

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

/* returns line number on error */
static int edit_read_syntax_rules (WEdit * edit, FILE * f)
{
    char *fg, *bg;
    char last_fg[32] = "", last_bg[32] = "";
    char whole_right[256];
    char whole_left[256];
    char *args[1024], *l = 0;
    int line = 0;
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
	if (!read_one_line (&l, f))
	    break;
	get_args (l, args, &argc);
	a = args + 1;
	if (!args[0]) {
	    /* do nothing */
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

#define CURRENT_SYNTAX_RULES_VERSION "44"

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
"###############################################################################",
"file ..\\*\\\\.diff$ Unified\\sDiff\\sOutput ^diff.\\*(-u|--unified)",
"# yawn",
"context default",
"    keyword linestart @@*@@	green/16",
"    keyword linestart \\s black/0 white/26",
"context linestart diff \\n white/26 red/9",
"context linestart --- \\n brightmagenta/20",
"context linestart +++ \\n brightmagenta/20",
"context linestart + \\n brightgreen/6",
"context linestart - \\n brightred/18",
"context linestart A \\n white/26 black/0",
"context linestart B \\n white/26 black/0",
"context linestart C \\n white/26 black/0",
"context linestart D \\n white/26 black/0",
"context linestart E \\n white/26 black/0",
"context linestart F \\n white/26 black/0",
"context linestart G \\n white/26 black/0",
"context linestart H \\n white/26 black/0",
"context linestart I \\n white/26 black/0",
"context linestart J \\n white/26 black/0",
"context linestart K \\n white/26 black/0",
"context linestart L \\n white/26 black/0",
"context linestart M \\n white/26 black/0",
"context linestart N \\n white/26 black/0",
"context linestart O \\n white/26 black/0",
"context linestart P \\n white/26 black/0",
"context linestart Q \\n white/26 black/0",
"context linestart R \\n white/26 black/0",
"context linestart S \\n white/26 black/0",
"context linestart T \\n white/26 black/0",
"context linestart U \\n white/26 black/0",
"context linestart V \\n white/26 black/0",
"context linestart W \\n white/26 black/0",
"context linestart X \\n white/26 black/0",
"context linestart Y \\n white/26 black/0",
"context linestart Z \\n white/26 black/0",
"",
"###############################################################################",
"file ..\\*\\\\.lsm$ LSM\\sFile",
"",
"context default",
"    keyword linestart Begin3		brightmagenta/20",
"    keyword linestart Title:\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Version:\\s\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Entered-date:\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Description:\\s\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Keywords:\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Alternate-site:\\s	red/9  yellow/24",
"    keyword linestart Primary-site:\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Original-site:\\s\\s	red/9  yellow/24",
"    keyword linestart Platforms:\\s\\s\\s\\s\\s\\s	red/9  yellow/24",
"    keyword linestart Copying-policy:\\s	red/9  yellow/24",
"    keyword linestart End			brightmagenta/20",
"",
"    keyword linestart \\t\\t				white/26 yellow/24",
"    keyword linestart \\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s	white/26 yellow/24",
"    keyword whole GPL	green/6",
"    keyword whole BSD	green/6",
"    keyword whole Shareware	green/6",
"    keyword whole sunsite.unc.edu	green/6",
"    keyword wholeright \\s*.tar.gz	green/6",
"    keyword wholeright \\s*.lsm	green/6",
"",
"context linestart Author:\\s\\s\\s\\s\\s\\s\\s\\s\\s \\n brightred/19",
"    keyword whole \\s*@*\\s(*)		cyan/16",
"",
"context linestart Maintained-by:\\s\\s \\n brightred/19",
"    keyword whole \\s*@*\\s(*)		cyan/16",
"",
"###############################################################################",
"file ..\\*\\\\.sh$ Shell\\sScript ^#!\\s\\*/.\\*/(ksh|bash|sh|pdkzsh)$",
"",
"context default",
"    keyword whole for yellow/24",
"    keyword whole in yellow/24",
"    keyword whole do yellow/24",
"    keyword whole done yellow/24",
"    keyword whole select yellow/24",
"    keyword whole case yellow/24",
"    keyword whole esac yellow/24",
"    keyword whole if yellow/24",
"    keyword whole then yellow/24",
"    keyword whole elif yellow/24",
"    keyword whole else yellow/24",
"    keyword whole fi yellow/24",
"    keyword whole while yellow/24",
"    keyword whole until yellow/24",
"    keyword ;; brightred/18",
"    keyword \\\\\" brightred/18",
"    keyword \\\\' brightred/18",
"    keyword ` brightred/18",
"    keyword ; brightcyan/17",
"    keyword $(*) brightgreen/16",
"    keyword ${*} brightgreen/16",
"    keyword { brightcyan/14",
"    keyword } brightcyan/14",
"",
"    keyword whole linestart #!\\[\\s\\]/bin/\\[abkpct\\]sh brightcyan/17 black/0",
"",
"    keyword $\\* brightred/18",
"    keyword $@ brightred/18",
"    keyword $# brightred/18",
"    keyword $? brightred/18",
"    keyword $- brightred/18",
"    keyword $$ brightred/18",
"    keyword $! brightred/18",
"    keyword $_ brightred/18",
"",
"    keyword wholeright $\\[0123456789\\]0 brightred/18",
"    keyword wholeright $\\[0123456789\\]1 brightred/18",
"    keyword wholeright $\\[0123456789\\]2 brightred/18",
"    keyword wholeright $\\[0123456789\\]3 brightred/18",
"    keyword wholeright $\\[0123456789\\]4 brightred/18",
"    keyword wholeright $\\[0123456789\\]5 brightred/18",
"    keyword wholeright $\\[0123456789\\]6 brightred/18",
"    keyword wholeright $\\[0123456789\\]7 brightred/18",
"    keyword wholeright $\\[0123456789\\]8 brightred/18",
"    keyword wholeright $\\[0123456789\\]9 brightred/18",
"",
"    keyword wholeright $+A brightgreen/16",
"    keyword wholeright $+B brightgreen/16",
"    keyword wholeright $+C brightgreen/16",
"    keyword wholeright $+D brightgreen/16",
"    keyword wholeright $+E brightgreen/16",
"    keyword wholeright $+F brightgreen/16",
"    keyword wholeright $+G brightgreen/16",
"    keyword wholeright $+H brightgreen/16",
"    keyword wholeright $+I brightgreen/16",
"    keyword wholeright $+J brightgreen/16",
"    keyword wholeright $+K brightgreen/16",
"    keyword wholeright $+L brightgreen/16",
"    keyword wholeright $+M brightgreen/16",
"    keyword wholeright $+N brightgreen/16",
"    keyword wholeright $+O brightgreen/16",
"    keyword wholeright $+P brightgreen/16",
"    keyword wholeright $+Q brightgreen/16",
"    keyword wholeright $+R brightgreen/16",
"    keyword wholeright $+S brightgreen/16",
"    keyword wholeright $+T brightgreen/16",
"    keyword wholeright $+U brightgreen/16",
"    keyword wholeright $+V brightgreen/16",
"    keyword wholeright $+W brightgreen/16",
"    keyword wholeright $+X brightgreen/16",
"    keyword wholeright $+Y brightgreen/16",
"    keyword wholeright $+Z brightgreen/16",
"",
"    keyword wholeright $+a brightgreen/16",
"    keyword wholeright $+b brightgreen/16",
"    keyword wholeright $+c brightgreen/16",
"    keyword wholeright $+d brightgreen/16",
"    keyword wholeright $+e brightgreen/16",
"    keyword wholeright $+f brightgreen/16",
"    keyword wholeright $+g brightgreen/16",
"    keyword wholeright $+h brightgreen/16",
"    keyword wholeright $+i brightgreen/16",
"    keyword wholeright $+j brightgreen/16",
"    keyword wholeright $+k brightgreen/16",
"    keyword wholeright $+l brightgreen/16",
"    keyword wholeright $+m brightgreen/16",
"    keyword wholeright $+n brightgreen/16",
"    keyword wholeright $+o brightgreen/16",
"    keyword wholeright $+p brightgreen/16",
"    keyword wholeright $+q brightgreen/16",
"    keyword wholeright $+r brightgreen/16",
"    keyword wholeright $+s brightgreen/16",
"    keyword wholeright $+t brightgreen/16",
"    keyword wholeright $+u brightgreen/16",
"    keyword wholeright $+v brightgreen/16",
"    keyword wholeright $+w brightgreen/16",
"    keyword wholeright $+x brightgreen/16",
"    keyword wholeright $+y brightgreen/16",
"    keyword wholeright $+z brightgreen/16",
"",
"    keyword wholeright $+0 brightgreen/16",
"    keyword wholeright $+1 brightgreen/16",
"    keyword wholeright $+2 brightgreen/16",
"    keyword wholeright $+3 brightgreen/16",
"    keyword wholeright $+4 brightgreen/16",
"    keyword wholeright $+5 brightgreen/16",
"    keyword wholeright $+6 brightgreen/16",
"    keyword wholeright $+7 brightgreen/16",
"    keyword wholeright $+8 brightgreen/16",
"    keyword wholeright $+9 brightgreen/16",
"",
"    keyword $ brightgreen/16",
"",
"    keyword wholeleft function*() brightblue/11",
"",
"wholechars right abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._",
"wholechars left abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._",
"",
"    keyword whole arch cyan/14",
"    keyword whole ash cyan/14",
"    keyword whole awk cyan/14",
"    keyword whole basename cyan/14",
"    keyword whole bash cyan/14",
"    keyword whole basnemae cyan/14",
"    keyword whole bg_backup cyan/14",
"    keyword whole bg_restore cyan/14",
"    keyword whole bsh cyan/14",
"    keyword whole cat cyan/14",
"    keyword whole chgrp cyan/14",
"    keyword whole chmod cyan/14",
"    keyword whole chown cyan/14",
"    keyword whole cp cyan/14",
"    keyword whole cpio cyan/14",
"    keyword whole csh cyan/14",
"    keyword whole date cyan/14",
"    keyword whole dd cyan/14",
"    keyword whole df cyan/14",
"    keyword whole dmesg cyan/14",
"    keyword whole dnsdomainname cyan/14",
"    keyword whole doexec cyan/14",
"    keyword whole domainname cyan/14",
"    keyword whole echo cyan/14",
"    keyword whole ed cyan/14",
"    keyword whole egrep cyan/14",
"    keyword whole ex cyan/14",
"    keyword whole false cyan/14",
"    keyword whole fgrep cyan/14",
"    keyword whole fsconf cyan/14",
"    keyword whole gawk cyan/14",
"    keyword whole grep cyan/14",
"    keyword whole gunzip cyan/14",
"    keyword whole gzip cyan/14",
"    keyword whole hostname cyan/14",
"    keyword whole igawk cyan/14",
"    keyword whole ipcalc cyan/14",
"    keyword whole kill cyan/14",
"    keyword whole ksh cyan/14",
"    keyword whole linuxconf cyan/14",
"    keyword whole ln cyan/14",
"    keyword whole login cyan/14",
"    keyword whole lpdconf cyan/14",
"    keyword whole ls cyan/14",
"    keyword whole mail cyan/14",
"    keyword whole mkdir cyan/14",
"    keyword whole mknod cyan/14",
"    keyword whole mktemp cyan/14",
"    keyword whole more cyan/14",
"    keyword whole mount cyan/14",
"    keyword whole mt cyan/14",
"    keyword whole mv cyan/14",
"    keyword whole netconf cyan/14",
"    keyword whole netstat cyan/14",
"    keyword whole nice cyan/14",
"    keyword whole nisdomainname cyan/14",
"    keyword whole ping cyan/14",
"    keyword whole ps cyan/14",
"    keyword whole pwd cyan/14",
"    keyword whole red cyan/14",
"    keyword whole remadmin cyan/14",
"    keyword whole rm cyan/14",
"    keyword whole rmdir cyan/14",
"    keyword whole rpm cyan/14",
"    keyword whole sed cyan/14",
"    keyword whole setserial cyan/14",
"    keyword whole sh cyan/14",
"    keyword whole sleep cyan/14",
"    keyword whole sort cyan/14",
"    keyword whole stty cyan/14",
"    keyword whole su cyan/14",
"    keyword whole sync cyan/14",
"    keyword whole taper cyan/14",
"    keyword whole tar cyan/14",
"    keyword whole tcsh cyan/14",
"    keyword whole touch cyan/14",
"    keyword whole true cyan/14",
"    keyword whole umount cyan/14",
"    keyword whole uname cyan/14",
"    keyword whole userconf cyan/14",
"    keyword whole usleep cyan/14",
"    keyword whole vi cyan/14",
"    keyword whole view cyan/14",
"    keyword whole vim cyan/14",
"    keyword whole xconf cyan/14",
"    keyword whole ypdomainname cyan/14",
"    keyword whole zcat cyan/14",
"    keyword whole zsh cyan/14",
"",
"wholechars right abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",
"wholechars left abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",
"",
"context # \\n brown/22",
"",
"context exclusive whole <\\[\\s\\\\\\]EOF EOF green/6",
"",
"context ' ' green/6",
"",
"context \" \" green/6",
"    keyword \\\\' brightgreen/16",
"    keyword \\\\\" brightgreen/16",
"    keyword $(*) brightgreen/16",
"    keyword ${*} brightgreen/16",
"    keyword $\\* brightred/18",
"    keyword $@ brightred/18",
"    keyword $# brightred/18",
"    keyword $? brightred/18",
"    keyword $- brightred/18",
"    keyword $$ brightred/18",
"    keyword $! brightred/18",
"    keyword $_ brightred/18",
"    keyword wholeright $\\[0123456789\\]0 brightred/18",
"    keyword wholeright $\\[0123456789\\]1 brightred/18",
"    keyword wholeright $\\[0123456789\\]2 brightred/18",
"    keyword wholeright $\\[0123456789\\]3 brightred/18",
"    keyword wholeright $\\[0123456789\\]4 brightred/18",
"    keyword wholeright $\\[0123456789\\]5 brightred/18",
"    keyword wholeright $\\[0123456789\\]6 brightred/18",
"    keyword wholeright $\\[0123456789\\]7 brightred/18",
"    keyword wholeright $\\[0123456789\\]8 brightred/18",
"    keyword wholeright $\\[0123456789\\]9 brightred/18",
"",
"    keyword wholeright $+A brightgreen/16",
"    keyword wholeright $+B brightgreen/16",
"    keyword wholeright $+C brightgreen/16",
"    keyword wholeright $+D brightgreen/16",
"    keyword wholeright $+E brightgreen/16",
"    keyword wholeright $+F brightgreen/16",
"    keyword wholeright $+G brightgreen/16",
"    keyword wholeright $+H brightgreen/16",
"    keyword wholeright $+I brightgreen/16",
"    keyword wholeright $+J brightgreen/16",
"    keyword wholeright $+K brightgreen/16",
"    keyword wholeright $+L brightgreen/16",
"    keyword wholeright $+M brightgreen/16",
"    keyword wholeright $+N brightgreen/16",
"    keyword wholeright $+O brightgreen/16",
"    keyword wholeright $+P brightgreen/16",
"    keyword wholeright $+Q brightgreen/16",
"    keyword wholeright $+R brightgreen/16",
"    keyword wholeright $+S brightgreen/16",
"    keyword wholeright $+T brightgreen/16",
"    keyword wholeright $+U brightgreen/16",
"    keyword wholeright $+V brightgreen/16",
"    keyword wholeright $+W brightgreen/16",
"    keyword wholeright $+X brightgreen/16",
"    keyword wholeright $+Y brightgreen/16",
"    keyword wholeright $+Z brightgreen/16",
"",
"    keyword wholeright $+a brightgreen/16",
"    keyword wholeright $+b brightgreen/16",
"    keyword wholeright $+c brightgreen/16",
"    keyword wholeright $+d brightgreen/16",
"    keyword wholeright $+e brightgreen/16",
"    keyword wholeright $+f brightgreen/16",
"    keyword wholeright $+g brightgreen/16",
"    keyword wholeright $+h brightgreen/16",
"    keyword wholeright $+i brightgreen/16",
"    keyword wholeright $+j brightgreen/16",
"    keyword wholeright $+k brightgreen/16",
"    keyword wholeright $+l brightgreen/16",
"    keyword wholeright $+m brightgreen/16",
"    keyword wholeright $+n brightgreen/16",
"    keyword wholeright $+o brightgreen/16",
"    keyword wholeright $+p brightgreen/16",
"    keyword wholeright $+q brightgreen/16",
"    keyword wholeright $+r brightgreen/16",
"    keyword wholeright $+s brightgreen/16",
"    keyword wholeright $+t brightgreen/16",
"    keyword wholeright $+u brightgreen/16",
"    keyword wholeright $+v brightgreen/16",
"    keyword wholeright $+w brightgreen/16",
"    keyword wholeright $+x brightgreen/16",
"    keyword wholeright $+y brightgreen/16",
"    keyword wholeright $+z brightgreen/16",
"",
"    keyword wholeright $+0 brightgreen/16",
"    keyword wholeright $+1 brightgreen/16",
"    keyword wholeright $+2 brightgreen/16",
"    keyword wholeright $+3 brightgreen/16",
"    keyword wholeright $+4 brightgreen/16",
"    keyword wholeright $+5 brightgreen/16",
"    keyword wholeright $+6 brightgreen/16",
"    keyword wholeright $+7 brightgreen/16",
"    keyword wholeright $+8 brightgreen/16",
"    keyword wholeright $+9 brightgreen/16",
"",
"    keyword $ brightgreen/16",
"",
"context exclusive ` ` white/26 black/0",
"    keyword '*' green/6",
"    keyword \" green/6",
"    keyword ; brightcyan/17",
"    keyword $(*) brightgreen/16",
"    keyword ${*} brightgreen/16",
"    keyword { brightcyan/14",
"    keyword } brightcyan/14",
"",
"    keyword $\\* brightred/18",
"    keyword $@ brightred/18",
"    keyword $# brightred/18",
"    keyword $? brightred/18",
"    keyword $- brightred/18",
"    keyword $$ brightred/18",
"    keyword $! brightred/18",
"    keyword $_ brightred/18",
"",
"    keyword wholeright $\\[0123456789\\]0 brightred/18",
"    keyword wholeright $\\[0123456789\\]1 brightred/18",
"    keyword wholeright $\\[0123456789\\]2 brightred/18",
"    keyword wholeright $\\[0123456789\\]3 brightred/18",
"    keyword wholeright $\\[0123456789\\]4 brightred/18",
"    keyword wholeright $\\[0123456789\\]5 brightred/18",
"    keyword wholeright $\\[0123456789\\]6 brightred/18",
"    keyword wholeright $\\[0123456789\\]7 brightred/18",
"    keyword wholeright $\\[0123456789\\]8 brightred/18",
"    keyword wholeright $\\[0123456789\\]9 brightred/18",
"",
"    keyword wholeright $+A brightgreen/16",
"    keyword wholeright $+B brightgreen/16",
"    keyword wholeright $+C brightgreen/16",
"    keyword wholeright $+D brightgreen/16",
"    keyword wholeright $+E brightgreen/16",
"    keyword wholeright $+F brightgreen/16",
"    keyword wholeright $+G brightgreen/16",
"    keyword wholeright $+H brightgreen/16",
"    keyword wholeright $+I brightgreen/16",
"    keyword wholeright $+J brightgreen/16",
"    keyword wholeright $+K brightgreen/16",
"    keyword wholeright $+L brightgreen/16",
"    keyword wholeright $+M brightgreen/16",
"    keyword wholeright $+N brightgreen/16",
"    keyword wholeright $+O brightgreen/16",
"    keyword wholeright $+P brightgreen/16",
"    keyword wholeright $+Q brightgreen/16",
"    keyword wholeright $+R brightgreen/16",
"    keyword wholeright $+S brightgreen/16",
"    keyword wholeright $+T brightgreen/16",
"    keyword wholeright $+U brightgreen/16",
"    keyword wholeright $+V brightgreen/16",
"    keyword wholeright $+W brightgreen/16",
"    keyword wholeright $+X brightgreen/16",
"    keyword wholeright $+Y brightgreen/16",
"    keyword wholeright $+Z brightgreen/16",
"",
"    keyword wholeright $+a brightgreen/16",
"    keyword wholeright $+b brightgreen/16",
"    keyword wholeright $+c brightgreen/16",
"    keyword wholeright $+d brightgreen/16",
"    keyword wholeright $+e brightgreen/16",
"    keyword wholeright $+f brightgreen/16",
"    keyword wholeright $+g brightgreen/16",
"    keyword wholeright $+h brightgreen/16",
"    keyword wholeright $+i brightgreen/16",
"    keyword wholeright $+j brightgreen/16",
"    keyword wholeright $+k brightgreen/16",
"    keyword wholeright $+l brightgreen/16",
"    keyword wholeright $+m brightgreen/16",
"    keyword wholeright $+n brightgreen/16",
"    keyword wholeright $+o brightgreen/16",
"    keyword wholeright $+p brightgreen/16",
"    keyword wholeright $+q brightgreen/16",
"    keyword wholeright $+r brightgreen/16",
"    keyword wholeright $+s brightgreen/16",
"    keyword wholeright $+t brightgreen/16",
"    keyword wholeright $+u brightgreen/16",
"    keyword wholeright $+v brightgreen/16",
"    keyword wholeright $+w brightgreen/16",
"    keyword wholeright $+x brightgreen/16",
"    keyword wholeright $+y brightgreen/16",
"    keyword wholeright $+z brightgreen/16",
"",
"    keyword wholeright $+0 brightgreen/16",
"    keyword wholeright $+1 brightgreen/16",
"    keyword wholeright $+2 brightgreen/16",
"    keyword wholeright $+3 brightgreen/16",
"    keyword wholeright $+4 brightgreen/16",
"    keyword wholeright $+5 brightgreen/16",
"    keyword wholeright $+6 brightgreen/16",
"    keyword wholeright $+7 brightgreen/16",
"    keyword wholeright $+8 brightgreen/16",
"    keyword wholeright $+9 brightgreen/16",
"",
"    keyword $ brightgreen/16",
"",
"wholechars right abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._",
"wholechars left abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._",
"",
"    keyword whole arch cyan/14",
"    keyword whole ash cyan/14",
"    keyword whole awk cyan/14",
"    keyword whole basename cyan/14",
"    keyword whole bash cyan/14",
"    keyword whole basnemae cyan/14",
"    keyword whole bg_backup cyan/14",
"    keyword whole bg_restore cyan/14",
"    keyword whole bsh cyan/14",
"    keyword whole cat cyan/14",
"    keyword whole chgrp cyan/14",
"    keyword whole chmod cyan/14",
"    keyword whole chown cyan/14",
"    keyword whole cp cyan/14",
"    keyword whole cpio cyan/14",
"    keyword whole csh cyan/14",
"    keyword whole date cyan/14",
"    keyword whole dd cyan/14",
"    keyword whole df cyan/14",
"    keyword whole dmesg cyan/14",
"    keyword whole dnsdomainname cyan/14",
"    keyword whole doexec cyan/14",
"    keyword whole domainname cyan/14",
"    keyword whole echo cyan/14",
"    keyword whole ed cyan/14",
"    keyword whole egrep cyan/14",
"    keyword whole ex cyan/14",
"    keyword whole false cyan/14",
"    keyword whole fgrep cyan/14",
"    keyword whole fsconf cyan/14",
"    keyword whole gawk cyan/14",
"    keyword whole grep cyan/14",
"    keyword whole gunzip cyan/14",
"    keyword whole gzip cyan/14",
"    keyword whole hostname cyan/14",
"    keyword whole igawk cyan/14",
"    keyword whole ipcalc cyan/14",
"    keyword whole kill cyan/14",
"    keyword whole ksh cyan/14",
"    keyword whole linuxconf cyan/14",
"    keyword whole ln cyan/14",
"    keyword whole login cyan/14",
"    keyword whole lpdconf cyan/14",
"    keyword whole ls cyan/14",
"    keyword whole mail cyan/14",
"    keyword whole mkdir cyan/14",
"    keyword whole mknod cyan/14",
"    keyword whole mktemp cyan/14",
"    keyword whole more cyan/14",
"    keyword whole mount cyan/14",
"    keyword whole mt cyan/14",
"    keyword whole mv cyan/14",
"    keyword whole netconf cyan/14",
"    keyword whole netstat cyan/14",
"    keyword whole nice cyan/14",
"    keyword whole nisdomainname cyan/14",
"    keyword whole ping cyan/14",
"    keyword whole ps cyan/14",
"    keyword whole pwd cyan/14",
"    keyword whole red cyan/14",
"    keyword whole remadmin cyan/14",
"    keyword whole rm cyan/14",
"    keyword whole rmdir cyan/14",
"    keyword whole rpm cyan/14",
"    keyword whole sed cyan/14",
"    keyword whole setserial cyan/14",
"    keyword whole sh cyan/14",
"    keyword whole sleep cyan/14",
"    keyword whole sort cyan/14",
"    keyword whole stty cyan/14",
"    keyword whole su cyan/14",
"    keyword whole sync cyan/14",
"    keyword whole taper cyan/14",
"    keyword whole tar cyan/14",
"    keyword whole tcsh cyan/14",
"    keyword whole touch cyan/14",
"    keyword whole true cyan/14",
"    keyword whole umount cyan/14",
"    keyword whole uname cyan/14",
"    keyword whole userconf cyan/14",
"    keyword whole usleep cyan/14",
"    keyword whole vi cyan/14",
"    keyword whole view cyan/14",
"    keyword whole vim cyan/14",
"    keyword whole xconf cyan/14",
"    keyword whole ypdomainname cyan/14",
"    keyword whole zcat cyan/14",
"    keyword whole zsh cyan/14",
"",
"###############################################################################",
"file ..\\*\\\\.(pl|PL])$ Perl\\sProgram ^#!\\s\\*/.\\*/perl$",
"context default",
"    keyword whole linestart #!\\[\\s\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0",
"    keyword whole linestart #!\\[\\s\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0",
"    keyword whole linestart #!\\[\\s\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0",
"    keyword whole linestart #!\\[\\s\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0",
"    keyword whole linestart #!\\[\\s\\]/bin/perl brightcyan/17 black/0",
"",
"    keyword $_ red/orange",
"    keyword $. red/orange",
"    keyword $/ red/orange",
"    keyword $, red/orange",
"    keyword $\" red/orange",
"    keyword $\\\\ red/orange",
"    keyword $# red/orange",
"    keyword $\\* red/orange",
"    keyword $? red/orange",
"    keyword $] red/orange",
"    keyword $[ red/orange",
"    keyword $; red/orange",
"    keyword $! red/orange",
"    keyword $@ red/orange",
"    keyword $: red/orange",
"    keyword $0 red/orange",
"    keyword $$ red/orange",
"    keyword $< red/orange",
"    keyword $> red/orange",
"    keyword $( red/orange",
"    keyword $) red/orange",
"",
"    keyword $% red/orange",
"    keyword $= red/orange",
"    keyword $- red/orange",
"    keyword $~ red/orange",
"    keyword $| red/orange",
"    keyword $& red/orange",
"    keyword $` red/orange",
"    keyword $' red/orange",
"    keyword $+ red/orange",
"    keyword $11 red/orange",
"    keyword $12 red/orange",
"    keyword $13 red/orange",
"    keyword $14 red/orange",
"    keyword $15 red/orange",
"    keyword $16 red/orange",
"    keyword $17 red/orange",
"    keyword $18 red/orange",
"    keyword $19 red/orange",
"    keyword $20 red/orange",
"    keyword $10 red/orange",
"    keyword $1 red/orange",
"    keyword $2 red/orange",
"    keyword $3 red/orange",
"    keyword $4 red/orange",
"    keyword $5 red/orange",
"    keyword $6 red/orange",
"    keyword $7 red/orange",
"    keyword $8 red/orange",
"    keyword $9 red/orange",
"    keyword $0 red/orange",
"",
"    keyword $^A red/orange",
"    keyword $^D red/orange",
"    keyword $^E red/orange",
"    keyword $^I red/orange",
"    keyword $^L red/orange",
"    keyword $^P red/orange",
"    keyword $^T red/orange",
"    keyword $^W red/orange",
"    keyword $^X red/orange",
"    keyword $^A red/orange",
"",
"    keyword @EXPORT red/orange",
"    keyword @EXPORT_OK red/orange",
"    keyword @INC red/orange",
"    keyword @ISA red/orange",
"    keyword @_ red/orange",
"    keyword @ENV red/orange",
"    keyword @OVERLOAD red/orange",
"    keyword @SIG red/orange",
"",
"    keyword $ brightgreen/15",
"    keyword & brightmagenta/19",
"    keyword % brightcyan/17",
"    keyword @ white/21",
"    keyword \\\\\" brightred/18",
"    keyword \\\\' brightred/18",
"",
"    keyword <+> brightred/18",
"    keyword -> yellow/24",
"    keyword => yellow/24",
"    keyword > yellow/24",
"    keyword < yellow/24",
"    keyword \\+ yellow/24",
"    keyword - yellow/24",
"    keyword \\* yellow/24",
"    keyword / yellow/24",
"    keyword % yellow/24",
"    keyword = yellow/24",
"    keyword != yellow/24",
"    keyword == yellow/24",
"    keyword whole ge yellow/24",
"    keyword whole le yellow/24",
"    keyword whole gt yellow/24",
"    keyword whole lt yellow/24",
"    keyword whole eq yellow/24",
"    keyword whole ne yellow/24",
"    keyword whole cmp yellow/24",
"    keyword ~ yellow/24",
"    keyword { brightcyan/14",
"    keyword } brightcyan/14",
"    keyword ( brightcyan/15",
"    keyword ) brightcyan/15",
"    keyword [ brightcyan/14",
"    keyword ] brightcyan/14",
"    keyword , brightcyan/14",
"    keyword .. brightcyan/14",
"    keyword : brightcyan/14",
"    keyword ; brightmagenta/19",
"",
"    keyword whole sub yellow/24",
"    keyword whole STDIN brightred/18",
"    keyword whole STDOUT brightred/18",
"    keyword whole STDERR brightred/18",
"    keyword whole STDARGV brightred/18",
"    keyword whole DATA brightred/18",
"",
"    keyword whole do magenta/23",
"    keyword whole if magenta/23",
"    keyword whole until magenta/23",
"    keyword whole elsif magenta/23",
"    keyword whole else magenta/23",
"    keyword whole unless magenta/23",
"    keyword whole while magenta/23",
"    keyword whole foreach magenta/23",
"    keyword whole goto magenta/23",
"    keyword whole last magenta/23",
"    keyword whole next magenta/23",
"    keyword whole bless magenta/23",
"    keyword whole caller magenta/23",
"    keyword whole import magenta/23",
"    keyword whole package magenta/23",
"    keyword whole require magenta/23",
"    keyword whole return magenta/23",
"    keyword whole untie magenta/23",
"    keyword whole use magenta/23",
"",
"    keyword whole diagnostics brightcyan/17",
"    keyword whole integer brightcyan/17",
"    keyword whole less brightcyan/17",
"    keyword whole lib brightcyan/17",
"    keyword whole ops brightcyan/17",
"    keyword whole overload brightcyan/17",
"    keyword whole sigtrap brightcyan/17",
"    keyword whole strict brightcyan/17",
"    keyword whole vars brightcyan/17",
"",
"    keyword whole abs yellow/24",
"    keyword whole atan2 yellow/24",
"    keyword whole cos yellow/24",
"    keyword whole exp yellow/24",
"    keyword whole int yellow/24",
"    keyword whole log yellow/24",
"    keyword whole rand yellow/24",
"    keyword whole sin yellow/24",
"    keyword whole sqrt yellow/24",
"    keyword whole srand yellow/24",
"    keyword whole time yellow/24",
"    keyword whole chr yellow/24",
"    keyword whole gmtime yellow/24",
"    keyword whole hex yellow/24",
"    keyword whole localtime yellow/24",
"    keyword whole oct yellow/24",
"    keyword whole ord yellow/24",
"    keyword whole vec yellow/24",
"    keyword whole pack yellow/24",
"    keyword whole unpack yellow/24",
"",
"    keyword whole chomp yellow/YellowGreen",
"    keyword whole chop yellow/YellowGreen",
"    keyword whole crypt yellow/YellowGreen",
"    keyword whole eval yellow/YellowGreen",
"    keyword whole index yellow/YellowGreen",
"    keyword whole length yellow/YellowGreen",
"    keyword whole lc yellow/YellowGreen",
"    keyword whole lcfirst yellow/YellowGreen",
"    keyword whole quotemeta yellow/YellowGreen",
"    keyword whole rindex yellow/YellowGreen",
"    keyword whole substr yellow/YellowGreen",
"    keyword whole uc yellow/YellowGreen",
"    keyword whole ucfirst yellow/YellowGreen",
"",
"    keyword whole delete yellow/24",
"    keyword whole each yellow/24",
"    keyword whole exists yellow/24",
"    keyword whole grep yellow/24",
"    keyword whole join yellow/24",
"    keyword whole keys yellow/24",
"    keyword whole map yellow/24",
"    keyword whole pop yellow/24",
"    keyword whole push yellow/24",
"    keyword whole reverse yellow/24",
"    keyword whole scalar yellow/24",
"    keyword whole shift yellow/24",
"    keyword whole sort yellow/24",
"    keyword whole splice yellow/24",
"    keyword whole split yellow/24",
"    keyword whole unshift yellow/24",
"    keyword whole values yellow/24",
"",
"    keyword whole chmod yellow/24",
"    keyword whole chown yellow/24",
"    keyword whole truncate yellow/24",
"    keyword whole link yellow/24",
"    keyword whole lstat yellow/24",
"    keyword whole mkdir yellow/24",
"    keyword whole readlink yellow/24",
"    keyword whole rename yellow/24",
"    keyword whole rmdir yellow/24",
"    keyword whole stat yellow/24",
"    keyword whole symlink yellow/24",
"    keyword whole unlink yellow/24",
"    keyword whole utime yellow/24",
"",
"    keyword whole binmade yellow/24",
"    keyword whole close yellow/24",
"    keyword whole dbmclose yellow/24",
"    keyword whole dbmopen yellow/24",
"    keyword whole binmade yellow/24",
"    keyword whole eof yellow/24",
"    keyword whole fcntl yellow/24",
"    keyword whole fileno yellow/24",
"    keyword whole flock yellow/24",
"    keyword whole getc yellow/24",
"    keyword whole ioctl yellow/24",
"    keyword whole open yellow/24",
"    keyword whole pipe yellow/24",
"    keyword whole print yellow/24",
"    keyword whole printf yellow/24",
"    keyword whole read yellow/24",
"    keyword whole seek yellow/24",
"    keyword whole select yellow/24",
"    keyword whole sprintf yellow/24",
"    keyword whole sysopen yellow/24",
"    keyword whole sysread yellow/24",
"    keyword whole syswrite yellow/24",
"    keyword whole tell yellow/24",
"",
"    keyword whole formline yellow/24",
"    keyword whole write yellow/24",
"",
"    keyword whole closedir yellow/24",
"    keyword whole opendir yellow/24",
"    keyword whole readdir yellow/24",
"    keyword whole rewinddir yellow/24",
"    keyword whole seekdir yellow/24",
"    keyword whole telldir yellow/24",
"",
"    keyword whole alarm yellow/24",
"    keyword whole chdir yellow/24",
"    keyword whole chroot yellow/24",
"    keyword whole die yellow/24",
"    keyword whole exec yellow/24",
"    keyword whole exit yellow/24",
"    keyword whole fork yellow/24",
"    keyword whole getlogin yellow/24",
"    keyword whole getpgrp yellow/24",
"    keyword whole getppid yellow/24",
"    keyword whole getpriority yellow/24",
"    keyword whole glob yellow/24",
"    keyword whole kill yellow/24",
"    keyword whole setpgrp yellow/24",
"    keyword whole setpriority yellow/24",
"    keyword whole sleep yellow/24",
"    keyword whole syscall yellow/24",
"    keyword whole system yellow/24",
"    keyword whole times yellow/24",
"    keyword whole umask yellow/24",
"    keyword whole wait yellow/24",
"    keyword whole waitpid yellow/24",
"    keyword whole warn yellow/24",
"",
"    keyword whole accept yellow/24",
"    keyword whole bind yellow/24",
"    keyword whole connect yellow/24",
"    keyword whole getpeername yellow/24",
"    keyword whole getsockname yellow/24",
"    keyword whole getsockopt yellow/24",
"    keyword whole listen yellow/24",
"    keyword whole recv yellow/24",
"    keyword whole send yellow/24",
"    keyword whole setsockopt yellow/24",
"    keyword whole shutdown yellow/24",
"    keyword whole socket yellow/24",
"    keyword whole socketpair yellow/24",
"",
"    keyword whole msgctl yellow/24",
"    keyword whole msgget yellow/24",
"    keyword whole msgsnd yellow/24",
"    keyword whole msgrcv yellow/24",
"    keyword whole semctl yellow/24",
"    keyword whole semget yellow/24",
"    keyword whole semop yellow/24",
"    keyword whole shmctl yellow/24",
"    keyword whole shmget yellow/24",
"    keyword whole shmread yellow/24",
"    keyword whole shmwrite yellow/24",
"",
"    keyword whole defined yellow/24",
"    keyword whole dump yellow/24",
"    keyword whole eval yellow/24",
"    keyword whole local yellow/24",
"    keyword whole my yellow/24",
"    keyword whole ref yellow/24",
"    keyword whole reset yellow/24",
"    keyword whole scalar yellow/24",
"    keyword whole undef yellow/24",
"    keyword whole wantarray yellow/24",
"",
"    keyword whole endpwent yellow/24",
"    keyword whole getpwent yellow/24",
"    keyword whole getpwnam yellow/24",
"    keyword whole getpwuid yellow/24",
"    keyword whole setpwent yellow/24",
"    keyword whole endgrent yellow/24",
"    keyword whole getgrgid yellow/24",
"    keyword whole getgrnam yellow/24",
"    keyword whole getgrent yellow/24",
"    keyword whole setgrent yellow/24",
"",
"    keyword whole endhostent yellow/24",
"    keyword whole gethostbyaddr yellow/24",
"    keyword whole gethostbyname yellow/24",
"    keyword whole gethostent yellow/24",
"    keyword whole sethostent yellow/24",
"",
"    keyword whole endnetent yellow/24",
"    keyword whole getnetbyaddr yellow/24",
"    keyword whole getnetbyname yellow/24",
"    keyword whole getnetent yellow/24",
"    keyword whole setnetent yellow/24",
"    keyword whole endservent yellow/24",
"    keyword whole getservbyname yellow/24",
"    keyword whole getservbyport yellow/24",
"    keyword whole getservent yellow/24",
"    keyword whole serservent yellow/24",
"    keyword whole endprotoent yellow/24",
"    keyword whole getprotobyname yellow/24",
"    keyword whole getprotobynumber yellow/24",
"    keyword whole getprotoent yellow/24",
"    keyword whole setprotoent yellow/24",
"",
"context exclusive whole <\\[\\s\\\\\\]EOF EOF green/6",
"context # \\n brown/22",
"context linestart = =cut brown/22",
"context \" \" green/6",
"    keyword \\\\\" brightgreen/16",
"    keyword \\\\\\\\ brightgreen/16",
"context ' ' brightgreen/16",
"    keyword \\\\' green/6",
"    keyword \\\\\\\\ green/6",
"",
"context exclusive ` ` white/26 black/0",
"",
"context whole __END__ guacomale_pudding white/26 black/0",
"",
"###############################################################################",
"file ..\\*\\\\.(py|PY])$ Python\\sProgram ^#!\\s\\*/.\\*/python$",
"context default",
"    keyword : brightred/18",
"    keyword > yellow/24",
"    keyword < yellow/24",
"    keyword \\+ yellow/24",
"    keyword - yellow/24",
"    keyword \\* yellow/24",
"    keyword / yellow/24",
"    keyword % yellow/24",
"    keyword = yellow/24",
"    keyword != yellow/24",
"    keyword == yellow/24",
"    keyword { brightcyan/14",
"    keyword } brightcyan/14",
"    keyword ( brightcyan/15",
"    keyword ) brightcyan/15",
"    keyword [ brightcyan/14",
"    keyword ] brightcyan/14",
"    keyword , brightcyan/14",
"    keyword : brightcyan/14",
"    keyword ; brightmagenta/19",
"    keyword whole self brightred/18",
"    keyword whole access yellow/24",
"    keyword whole and yellow/24",
"    keyword whole break yellow/24",
"    keyword whole class yellow/24",
"    keyword whole continue yellow/24",
"    keyword whole def yellow/24",
"    keyword whole del yellow/24",
"    keyword whole elif yellow/24",
"    keyword whole else yellow/24",
"    keyword whole except yellow/24",
"    keyword whole exec yellow/24",
"    keyword whole finally yellow/24",
"    keyword whole for yellow/24",
"    keyword whole from yellow/24",
"    keyword whole global yellow/24",
"    keyword whole if yellow/24",
"    keyword whole import yellow/24",
"    keyword whole in yellow/24",
"    keyword whole is yellow/24",
"    keyword whole lambda yellow/24",
"    keyword whole not yellow/24",
"    keyword whole or yellow/24",
"    keyword whole pass yellow/24",
"    keyword whole print yellow/24",
"    keyword whole raise yellow/24",
"    keyword whole return yellow/24",
"    keyword whole try yellow/24",
"    keyword whole while yellow/24",
"",
"    keyword whole abs brightcyan/17",
"    keyword whole apply brightcyan/17",
"    keyword whole callable brightcyan/17",
"    keyword whole chr brightcyan/17",
"    keyword whole cmp brightcyan/17",
"    keyword whole coerce brightcyan/17",
"    keyword whole compile brightcyan/17",
"    keyword whole delattr brightcyan/17",
"    keyword whole dir brightcyan/17",
"    keyword whole divmod brightcyan/17",
"    keyword whole eval brightcyan/17",
"    keyword whole exec brightcyan/17",
"    keyword whole execfile brightcyan/17",
"    keyword whole filter brightcyan/17",
"    keyword whole float brightcyan/17",
"    keyword whole getattr brightcyan/17",
"    keyword whole globals brightcyan/17",
"    keyword whole hasattr brightcyan/17",
"    keyword whole hash brightcyan/17",
"    keyword whole hex brightcyan/17",
"    keyword whole id brightcyan/17",
"    keyword whole input brightcyan/17",
"    keyword whole int brightcyan/17",
"    keyword whole len brightcyan/17",
"    keyword whole locals brightcyan/17",
"    keyword whole long brightcyan/17",
"    keyword whole map brightcyan/17",
"    keyword whole max brightcyan/17",
"    keyword whole min brightcyan/17",
"    keyword whole oct brightcyan/17",
"    keyword whole open brightcyan/17",
"    keyword whole ord brightcyan/17",
"    keyword whole pow brightcyan/17",
"    keyword whole range brightcyan/17",
"    keyword whole raw_input brightcyan/17",
"    keyword whole reduce brightcyan/17",
"    keyword whole reload brightcyan/17",
"    keyword whole repr brightcyan/17",
"    keyword whole round brightcyan/17",
"    keyword whole setattr brightcyan/17",
"    keyword whole str brightcyan/17",
"    keyword whole tuple brightcyan/17",
"    keyword whole type brightcyan/17",
"    keyword whole vars brightcyan/17",
"    keyword whole xrange brightcyan/17",
"",
"    keyword whole atof magenta/23",
"    keyword whole atoi magenta/23",
"    keyword whole atol magenta/23",
"    keyword whole expandtabs magenta/23",
"    keyword whole find magenta/23",
"    keyword whole rfind magenta/23",
"    keyword whole index magenta/23",
"    keyword whole rindex magenta/23",
"    keyword whole count magenta/23",
"    keyword whole split magenta/23",
"    keyword whole splitfields magenta/23",
"    keyword whole join magenta/23",
"    keyword whole joinfields magenta/23",
"    keyword whole strip magenta/23",
"    keyword whole swapcase magenta/23",
"    keyword whole upper magenta/23",
"    keyword whole lower magenta/23",
"    keyword whole ljust magenta/23",
"    keyword whole rjust magenta/23",
"    keyword whole center magenta/23",
"    keyword whole zfill magenta/23",
"",
"    keyword whole __init__ lightgray/13",
"    keyword whole __del__ lightgray/13",
"    keyword whole __repr__ lightgray/13",
"    keyword whole __str__ lightgray/13",
"    keyword whole __cmp__ lightgray/13",
"    keyword whole __hash__ lightgray/13",
"    keyword whole __call__ lightgray/13",
"    keyword whole __getattr__ lightgray/13",
"    keyword whole __setattr__ lightgray/13",
"    keyword whole __delattr__ lightgray/13",
"    keyword whole __len__ lightgray/13",
"    keyword whole __getitem__ lightgray/13",
"    keyword whole __setitem__ lightgray/13",
"    keyword whole __delitem__ lightgray/13",
"    keyword whole __getslice__ lightgray/13",
"    keyword whole __setslice__ lightgray/13",
"    keyword whole __delslice__ lightgray/13",
"    keyword whole __add__ lightgray/13",
"    keyword whole __sub__ lightgray/13",
"    keyword whole __mul__ lightgray/13",
"    keyword whole __div__ lightgray/13",
"    keyword whole __mod__ lightgray/13",
"    keyword whole __divmod__ lightgray/13",
"    keyword whole __pow__ lightgray/13",
"    keyword whole __lshift__ lightgray/13",
"    keyword whole __rshift__ lightgray/13",
"    keyword whole __and__ lightgray/13",
"    keyword whole __xor__ lightgray/13",
"    keyword whole __or__ lightgray/13",
"    keyword whole __neg__ lightgray/13",
"    keyword whole __pos__ lightgray/13",
"    keyword whole __abs__ lightgray/13",
"    keyword whole __invert__ lightgray/13",
"    keyword whole __nonzero__ lightgray/13",
"    keyword whole __coerce__ lightgray/13",
"    keyword whole __int__ lightgray/13",
"    keyword whole __long__ lightgray/13",
"    keyword whole __float__ lightgray/13",
"    keyword whole __oct__ lightgray/13",
"    keyword whole __hex__ lightgray/13",
"",
"    keyword whole __init__ lightgray/13",
"    keyword whole __del__ lightgray/13",
"    keyword whole __repr__ lightgray/13",
"    keyword whole __str__ lightgray/13",
"    keyword whole __cmp__ lightgray/13",
"    keyword whole __hash__ lightgray/13",
"    keyword whole __call__ lightgray/13",
"    keyword whole __getattr__ lightgray/13",
"    keyword whole __setattr__ lightgray/13",
"    keyword whole __delattr__ lightgray/13",
"    keyword whole __len__ lightgray/13",
"    keyword whole __getitem__ lightgray/13",
"    keyword whole __setitem__ lightgray/13",
"    keyword whole __delitem__ lightgray/13",
"    keyword whole __getslice__ lightgray/13",
"    keyword whole __setslice__ lightgray/13",
"    keyword whole __delslice__ lightgray/13",
"    keyword whole __add__ lightgray/13",
"    keyword whole __sub__ lightgray/13",
"    keyword whole __mul__ lightgray/13",
"    keyword whole __div__ lightgray/13",
"    keyword whole __mod__ lightgray/13",
"    keyword whole __divmod__ lightgray/13",
"    keyword whole __pow__ lightgray/13",
"    keyword whole __lshift__ lightgray/13",
"    keyword whole __rshift__ lightgray/13",
"    keyword whole __and__ lightgray/13",
"    keyword whole __xor__ lightgray/13",
"    keyword whole __or__ lightgray/13",
"    keyword whole __neg__ lightgray/13",
"    keyword whole __pos__ lightgray/13",
"    keyword whole __abs__ lightgray/13",
"    keyword whole __invert__ lightgray/13",
"    keyword whole __nonzero__ lightgray/13",
"    keyword whole __coerce__ lightgray/13",
"    keyword whole __int__ lightgray/13",
"    keyword whole __long__ lightgray/13",
"    keyword whole __float__ lightgray/13",
"    keyword whole __oct__ lightgray/13",
"    keyword whole __hex__ lightgray/13",
"",
"    keyword whole __radd__ lightgray/13",
"    keyword whole __rsub__ lightgray/13",
"    keyword whole __rmul__ lightgray/13",
"    keyword whole __rdiv__ lightgray/13",
"    keyword whole __rmod__ lightgray/13",
"    keyword whole __rdivmod__ lightgray/13",
"    keyword whole __rpow__ lightgray/13",
"    keyword whole __rlshift__ lightgray/13",
"    keyword whole __rrshift__ lightgray/13",
"    keyword whole __rand__ lightgray/13",
"    keyword whole __rxor__ lightgray/13",
"    keyword whole __ror__ lightgray/13",
"",
"    keyword whole __*__ brightred/18",
"",
"context \"\"\" \"\"\" brown/22",
"context # \\n brown/22",
"context \" \" green/6",
"    keyword \\\\\" brightgreen/16",
"    keyword %% brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16",
"    keyword %\\[hl\\]n brightgreen/16",
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16",
"    keyword %[*] brightgreen/16",
"    keyword %c brightgreen/16",
"    keyword \\\\\\\\ brightgreen/16",
"    keyword \\\\' brightgreen/16",
"    keyword \\\\a brightgreen/16",
"    keyword \\\\b brightgreen/16",
"    keyword \\\\t brightgreen/16",
"    keyword \\\\n brightgreen/16",
"    keyword \\\\v brightgreen/16",
"    keyword \\\\f brightgreen/16",
"    keyword \\\\r brightgreen/16",
"    keyword \\\\0 brightgreen/16",
"",
"context ' ' green/6",
"    keyword \\\\\" brightgreen/16",
"    keyword %% brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16",
"    keyword %\\[hl\\]n brightgreen/16",
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16",
"    keyword %[*] brightgreen/16",
"    keyword %c brightgreen/16",
"    keyword \\\\\\\\ brightgreen/16",
"    keyword \\\\' brightgreen/16",
"    keyword \\\\a brightgreen/16",
"    keyword \\\\b brightgreen/16",
"    keyword \\\\t brightgreen/16",
"    keyword \\\\n brightgreen/16",
"    keyword \\\\v brightgreen/16",
"    keyword \\\\f brightgreen/16",
"    keyword \\\\r brightgreen/16",
"    keyword \\\\0 brightgreen/16",
"",
"###############################################################################",
"file ..\\*\\\\.(man|[0-9n]|[0-9]x)$ NROFF\\sSource",
"",
"context default",
"    keyword \\\\fP brightgreen/6",
"    keyword \\\\fR brightgreen/6",
"    keyword \\\\fB brightgreen/6",
"    keyword \\\\fI brightgreen/6",
"    keyword linestart .AS cyan/5",
"    keyword linestart .Ar cyan/5",
"    keyword linestart .At cyan/5",
"    keyword linestart .BE cyan/5",
"    keyword linestart .BH cyan/5",
"    keyword linestart .BI cyan/5",
"    keyword linestart .BR cyan/5",
"    keyword linestart .BS cyan/5",
"    keyword linestart .Bd cyan/5",
"    keyword linestart .Bk cyan/5",
"    keyword linestart .Bl cyan/5",
"    keyword linestart .Bu cyan/5",
"    keyword linestart .Bx cyan/5",
"    keyword linestart .CE cyan/5",
"    keyword linestart .CM cyan/5",
"    keyword linestart .CS cyan/5",
"    keyword linestart .CT cyan/5",
"    keyword linestart .CW cyan/5",
"    keyword linestart .Cm cyan/5",
"    keyword linestart .Co cyan/5",
"    keyword linestart .DA cyan/5",
"    keyword linestart .DE cyan/5",
"    keyword linestart .DS cyan/5",
"    keyword linestart .DT cyan/5",
"    keyword linestart .Dd cyan/5",
"    keyword linestart .De cyan/5",
"    keyword linestart .Dl cyan/5",
"    keyword linestart .Dq cyan/5",
"    keyword linestart .Ds cyan/5",
"    keyword linestart .Dt cyan/5",
"    keyword linestart .Dv cyan/5",
"    keyword linestart .EE cyan/5",
"    keyword linestart .EN cyan/5",
"    keyword linestart .EQ cyan/5",
"    keyword linestart .EX cyan/5",
"    keyword linestart .Ed cyan/5",
"    keyword linestart .Ee cyan/5",
"    keyword linestart .Ek cyan/5",
"    keyword linestart .El cyan/5",
"    keyword linestart .Em cyan/5",
"    keyword linestart .En cyan/5",
"    keyword linestart .Ev cyan/5",
"    keyword linestart .Ex cyan/5",
"    keyword linestart .FI cyan/5",
"    keyword linestart .FL cyan/5",
"    keyword linestart .FN cyan/5",
"    keyword linestart .FT cyan/5",
"    keyword linestart .Fi cyan/5",
"    keyword linestart .Fl cyan/5",
"    keyword linestart .Fn cyan/5",
"    keyword linestart .HP cyan/5",
"    keyword linestart .HS cyan/5",
"    keyword linestart .Hh cyan/5",
"    keyword linestart .Hi cyan/5",
"    keyword linestart .IB cyan/5",
"    keyword linestart .IP cyan/5",
"    keyword linestart .IR cyan/5",
"    keyword linestart .IX cyan/5",
"    keyword linestart .Ic cyan/5",
"    keyword linestart .Id cyan/5",
"    keyword linestart .Ip cyan/5",
"    keyword linestart .It cyan/5",
"    keyword linestart .LI cyan/5",
"    keyword linestart .LO cyan/5",
"    keyword linestart .LP cyan/5",
"    keyword linestart .LR cyan/5",
"    keyword linestart .Li cyan/5",
"    keyword linestart .MF cyan/5",
"    keyword linestart .ML cyan/5",
"    keyword linestart .MU cyan/5",
"    keyword linestart .MV cyan/5",
"    keyword linestart .N cyan/5",
"    keyword linestart .NF cyan/5",
"    keyword linestart .Nd cyan/5",
"    keyword linestart .Nm cyan/5",
"    keyword linestart .No cyan/5",
"    keyword linestart .OP cyan/5",
"    keyword linestart .Oc cyan/5",
"    keyword linestart .Oo cyan/5",
"    keyword linestart .Op cyan/5",
"    keyword linestart .Os cyan/5",
"    keyword linestart .PD cyan/5",
"    keyword linestart .PN cyan/5",
"    keyword linestart .PP cyan/5",
"    keyword linestart .PU cyan/5",
"    keyword linestart .Pa cyan/5",
"    keyword linestart .Pf cyan/5",
"    keyword linestart .Pp cyan/5",
"    keyword linestart .Pq cyan/5",
"    keyword linestart .Pr cyan/5",
"    keyword linestart .Ps cyan/5",
"    keyword linestart .Ql cyan/5",
"    keyword linestart .RB cyan/5",
"    keyword linestart .RE cyan/5",
"    keyword linestart .RI cyan/5",
"    keyword linestart .RS cyan/5",
"    keyword linestart .RT cyan/5",
"    keyword linestart .Re cyan/5",
"    keyword linestart .Rs cyan/5",
"    keyword linestart .SB cyan/5",
"    keyword linestart .SH cyan/5",
"    keyword linestart .SM cyan/5",
"    keyword linestart .SP cyan/5",
"    keyword linestart .SS cyan/5",
"    keyword linestart .Sa cyan/5",
"    keyword linestart .Sh cyan/5",
"    keyword linestart .Sm cyan/5",
"    keyword linestart .Sp cyan/5",
"    keyword linestart .Sq cyan/5",
"    keyword linestart .Ss cyan/5",
"    keyword linestart .St cyan/5",
"    keyword linestart .Sx cyan/5",
"    keyword linestart .Sy cyan/5",
"    keyword linestart .TE cyan/5",
"    keyword linestart .TH cyan/5",
"    keyword linestart .TP cyan/5",
"    keyword linestart .TQ cyan/5",
"    keyword linestart .TS cyan/5",
"    keyword linestart .Tn cyan/5",
"    keyword linestart .Tp cyan/5",
"    keyword linestart .UC cyan/5",
"    keyword linestart .Uh cyan/5",
"    keyword linestart .Ux cyan/5",
"    keyword linestart .VE cyan/5",
"    keyword linestart .VS cyan/5",
"    keyword linestart .Va cyan/5",
"    keyword linestart .Vb cyan/5",
"    keyword linestart .Ve cyan/5",
"    keyword linestart .Xc cyan/5",
"    keyword linestart .Xe cyan/5",
"    keyword linestart .Xr cyan/5",
"    keyword linestart .YN cyan/5",
"    keyword linestart .ad cyan/5",
"    keyword linestart .am cyan/5",
"    keyword linestart .bd cyan/5",
"    keyword linestart .bp cyan/5",
"    keyword linestart .br cyan/5",
"    keyword linestart .ce cyan/5",
"    keyword linestart .cs cyan/5",
"    keyword linestart .de cyan/5",
"    keyword linestart .ds cyan/5",
"    keyword linestart .ec cyan/5",
"    keyword linestart .eh cyan/5",
"    keyword linestart .el cyan/5",
"    keyword linestart .eo cyan/5",
"    keyword linestart .ev cyan/5",
"    keyword linestart .fc cyan/5",
"    keyword linestart .fi cyan/5",
"    keyword linestart .ft cyan/5",
"    keyword linestart .hy cyan/5",
"    keyword linestart .iX cyan/5",
"    keyword linestart .ie cyan/5",
"    keyword linestart .if cyan/5",
"    keyword linestart .ig cyan/5",
"    keyword linestart .in cyan/5",
"    keyword linestart .ll cyan/5",
"    keyword linestart .lp cyan/5",
"    keyword linestart .ls cyan/5",
"    keyword linestart .mk cyan/5",
"    keyword linestart .na cyan/5",
"    keyword linestart .ne cyan/5",
"    keyword linestart .nf cyan/5",
"    keyword linestart .nh cyan/5",
"    keyword linestart .nr cyan/5",
"    keyword linestart .ns cyan/5",
"    keyword linestart .oh cyan/5",
"    keyword linestart .ps cyan/5",
"    keyword linestart .re cyan/5",
"    keyword linestart .rm cyan/5",
"    keyword linestart .rn cyan/5",
"    keyword linestart .rr cyan/5",
"    keyword linestart .so cyan/5",
"    keyword linestart .sp cyan/5",
"    keyword linestart .ss cyan/5",
"    keyword linestart .ta cyan/5",
"    keyword linestart .ti cyan/5",
"    keyword linestart .tm cyan/5",
"    keyword linestart .tr cyan/5",
"    keyword linestart .ul cyan/5",
"    keyword linestart .vs cyan/5",
"    keyword linestart .zZ cyan/5",
"    keyword linestart .e cyan/5",
"    keyword linestart .d cyan/5",
"    keyword linestart .h cyan/5",
"    keyword linestart .B cyan/5",
"    keyword linestart .I cyan/5",
"    keyword linestart .R cyan/5",
"    keyword linestart .P cyan/5",
"    keyword linestart .L cyan/5",
"    keyword linestart .V cyan/5",
"    keyword linestart .F cyan/5",
"    keyword linestart .T cyan/5",
"    keyword linestart .X cyan/5",
"    keyword linestart .Y cyan/5",
"    keyword linestart .b cyan/5",
"    keyword linestart .l cyan/5",
"    keyword linestart .i cyan/5",
"",
"context exclusive linestart .SH \\n brightred/18",
"context exclusive linestart .TH \\n brightred/18",
"context exclusive linestart .B \\n magenta/23",
"context exclusive linestart .I \\n yellow/24",
"context exclusive linestart .nf linestart .fi green/15",
"",
"# font changes should end in a \\fP",
"context exclusive \\\\fB \\\\fP magenta/23",
"context exclusive \\\\fI \\\\fP yellow/24",
"context linestart .\\\\\" \\n brown/22",
"",
"###############################################################################",
"# Assumes you've set a dark background, e.g. navy blue.",
"file ..\\*\\\\.(htm|html|HTM|HTML)$ HTML\\sFile",
"",
"context default white/25",
"    keyword whole &*; brightgreen/16",
"context <!-- --> white/26",
"context < > brightcyan/17",
"    keyword \"http:*\" magenta/22",
"    keyword \"ftp:*\" magenta/22",
"    keyword \"mailto:*\" magenta/22",
"    keyword \"gopher:*\" magenta/22",
"    keyword \"telnet:*\" magenta/22",
"    keyword \"file:*\" magenta/22",
"    keyword \"*.gif\" brightred/19",
"    keyword \"*.jpg\" brightred/19",
"    keyword \"*.png\" brightred/19",
"    keyword \"*\" cyan/5",
"",
"###############################################################################",
"# Pascal (BP7 IDE alike)",
"file ..\\*\\\\.(pp|PP|pas|PAS|)$ Pascal Program",
"context default yellow/24",
"    keyword whole absolute white/25",
"    keyword whole and white/25",
"    keyword whole array white/25",
"    keyword whole as white/25",
"    keyword whole asm white/25",
"    keyword whole assembler white/25",
"    keyword whole begin white/25",
"    keyword whole break white/25",
"    keyword whole case white/25",
"    keyword whole class white/25",
"    keyword whole const white/25",
"    keyword whole continue white/25",
"    keyword whole constructor white/25",
"    keyword whole destructor white/25",
"    keyword whole dispose white/25",
"    keyword whole div white/25",
"    keyword whole do white/25",
"    keyword whole downto white/25",
"    keyword whole else white/25",
"    keyword whole end white/25",
"    keyword whole except white/25",
"    keyword whole exit white/25",
"    keyword whole export white/25",
"    keyword whole exports white/25",
"    keyword whole external white/25",
"    keyword whole fail white/25",
"    keyword whole far white/25",
"    keyword whole false white/25",
"    keyword whole file white/25",
"    keyword whole finally white/25",
"    keyword whole for white/25",
"    keyword whole forward white/25",
"    keyword whole function white/25",
"    keyword whole goto white/25",
"    keyword whole if white/25",
"    keyword whole implementation white/25",
"    keyword whole in white/25",
"    keyword whole inherited white/25",
"    keyword whole initialization white/25",
"    keyword whole inline white/25",
"    keyword whole interface white/25",
"    keyword whole interrupt white/25",
"    keyword whole is white/25",
"    keyword whole label white/25",
"    keyword whole library white/25",
"    keyword whole mod white/25",
"    keyword whole near white/25",
"    keyword whole new white/25",
"    keyword whole nil white/25",
"    keyword whole not white/25",
"    keyword whole object white/25",
"    keyword whole of white/25",
"    keyword whole on white/25",
"    keyword whole operator white/25",
"    keyword whole or white/25",
"    keyword whole otherwise white/25",
"    keyword whole packed white/25",
"    keyword whole procedure white/25",
"    keyword whole program white/25",
"    keyword whole property white/25",
"    keyword whole raise white/25",
"    keyword whole record white/25",
"    keyword whole repeat white/25",
"    keyword whole self white/25",
"    keyword whole set white/25",
"    keyword whole shl white/25",
"    keyword whole shr white/25",
"    keyword whole string white/25",
"    keyword whole then white/25",
"    keyword whole to white/25",
"    keyword whole true white/25",
"    keyword whole try white/25",
"    keyword whole type white/25",
"    keyword whole unit white/25",
"    keyword whole until white/25",
"    keyword whole uses white/25",
"    keyword whole var white/25",
"    keyword whole virtual white/25",
"    keyword whole while white/25",
"    keyword whole with white/25",
"    keyword whole xor white/25",
"    keyword whole .. white/25",
"    ",
"    keyword > cyan/5",
"    keyword < cyan/5",
"    keyword \\+ cyan/5",
"    keyword - cyan/5",
"    keyword / cyan/5",
"    keyword % cyan/5",
"    keyword = cyan/5",
"    keyword [ cyan/5",
"    keyword ] cyan/5",
"    keyword ( cyan/5",
"    keyword ) cyan/5",
"    keyword , cyan/5",
"    keyword . cyan/5",
"    keyword : cyan/5",
"    keyword ; cyan/5",
"    keyword {$*} brightred/19",
"",
"context ' ' brightcyan/22",
"context // \\n brown/22",
"context (\\* \\*) brown/22",
"# context {$ } brightred/19",
"#    keyword \\[ABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\[-\\+\\] brightgreen/16",
"#    keyword $* brightgreen/16",
"context { } lightgray/22",
"",
"",
"###############################################################################",
"file ..\\*\\\\.tex$ LaTeX\\s2.09\\sDocument",
"context default",
"wholechars left \\\\ ",
"wholechars right abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
"",
"# type style",
"    keyword whole \\\\tiny yellow/24",
"    keyword whole \\\\scriptsize yellow/24",
"    keyword whole \\\\footnotesize yellow/24",
"    keyword whole \\\\small yellow/24",
"    keyword whole \\\\normalsize yellow/24",
"    keyword whole \\\\large yellow/24",
"    keyword whole \\\\Large yellow/24",
"    keyword whole \\\\LARGE yellow/24",
"    keyword whole \\\\huge yellow/24",
"    keyword whole \\\\Huge yellow/24",
"",
"# accents and symbols",
"    keyword whole \\\\`{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\'{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\^{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\\"{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\~{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\={\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\.{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\u{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\v{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\H{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\t{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\c{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\d{\\[aeiouAEIOU\\]} yellow/24",
"    keyword whole \\\\b{\\[aeiouAEIOU\\]} yellow/24",
"",
"    keyword whole \\\\dag yellow/24",
"    keyword whole \\\\ddag yellow/24",
"    keyword whole \\\\S yellow/24",
"    keyword whole \\\\P yellow/24",
"    keyword whole \\\\copyright yellow/24",
"    keyword whole \\\\pounds yellow/24",
"",
"# sectioning and table of contents",
"    keyword whole \\\\part[*]{*} brightred/19",
"    keyword whole \\\\part{*} brightred/19",
"    keyword whole \\\\part\\*{*} brightred/19",
"    keyword whole \\\\chapter[*]{*} brightred/19",
"    keyword whole \\\\chapter{*} brightred/19",
"    keyword whole \\\\chapter\\*{*} brightred/19",
"    keyword whole \\\\section[*]{*} brightred/19",
"    keyword whole \\\\section{*} brightred/19",
"    keyword whole \\\\section\\*{*} brightred/19",
"    keyword whole \\\\subsection[*]{*} brightred/19",
"    keyword whole \\\\subsection{*} brightred/19",
"    keyword whole \\\\subsection\\*{*} brightred/19",
"    keyword whole \\\\subsubsection[*]{*} brightred/19",
"    keyword whole \\\\subsubsection{*} brightred/19",
"    keyword whole \\\\subsubsection\\*{*} brightred/19",
"    keyword whole \\\\paragraph[*]{*} brightred/19",
"    keyword whole \\\\paragraph{*} brightred/19",
"    keyword whole \\\\paragraph\\*{*} brightred/19",
"    keyword whole \\\\subparagraph[*]{*} brightred/19",
"    keyword whole \\\\subparagraph{*} brightred/19",
"    keyword whole \\\\subparagraph\\*{*} brightred/19",
"",
"    keyword whole \\\\appendix brightred/19",
"    keyword whole \\\\tableofcontents brightred/19",
"",
"# misc",
"    keyword whole \\\\item[*] yellow/24",
"    keyword whole \\\\item yellow/24",
"    keyword whole \\\\\\\\ yellow/24",
"    keyword \\\\\\s yellow/24 black/0",
"    keyword %% yellow/24",
"",
"# docuement and page styles    ",
"    keyword whole \\\\documentstyle[*]{*} yellow/20",
"    keyword whole \\\\documentstyle{*} yellow/20",
"    keyword whole \\\\pagestyle{*} yellow/20",
"",
"# cross references",
"    keyword whole \\\\label{*} yellow/24",
"    keyword whole \\\\ref{*} yellow/24",
"",
"# bibliography and citations",
"    keyword whole \\\\bibliography{*} yellow/24",
"    keyword whole \\\\bibitem[*]{*} yellow/24",
"    keyword whole \\\\bibitem{*} yellow/24",
"    keyword whole \\\\cite[*]{*} yellow/24",
"    keyword whole \\\\cite{*} yellow/24",
"",
"# splitting the input",
"    keyword whole \\\\input{*} yellow/20",
"    keyword whole \\\\include{*} yellow/20",
"    keyword whole \\\\includeonly{*} yellow/20",
"",
"# line breaking",
"    keyword whole \\\\linebreak[\\[01234\\]] yellow/24",
"    keyword whole \\\\nolinebreak[\\[01234\\]] yellow/24",
"    keyword whole \\\\linebreak yellow/24",
"    keyword whole \\\\nolinebreak yellow/24",
"    keyword whole \\\\[+] yellow/24",
"    keyword whole \\\\- yellow/24",
"    keyword whole \\\\sloppy yellow/24",
"",
"# page breaking",
"    keyword whole \\\\pagebreak[\\[01234\\]] yellow/24",
"    keyword whole \\\\nopagebreak[\\[01234\\]] yellow/24",
"    keyword whole \\\\pagebreak yellow/24",
"    keyword whole \\\\nopagebreak yellow/24",
"    keyword whole \\\\samepage yellow/24",
"    keyword whole \\\\newpage yellow/24",
"    keyword whole \\\\clearpage yellow/24",
"",
"# defintiions",
"    keyword \\\\newcommand{*}[*] cyan/5",
"    keyword \\\\newcommand{*} cyan/5",
"    keyword \\\\newenvironment{*}[*]{*} cyan/5",
"    keyword \\\\newenvironment{*}{*} cyan/5",
"",
"# boxes",
"",
"# begins and ends",
"    keyword \\\\begin{document} brightred/14",
"    keyword \\\\begin{equation} brightred/14",
"    keyword \\\\begin{eqnarray} brightred/14",
"    keyword \\\\begin{quote} brightred/14",
"    keyword \\\\begin{quotation} brightred/14",
"    keyword \\\\begin{center} brightred/14",
"    keyword \\\\begin{verse} brightred/14",
"    keyword \\\\begin{verbatim} brightred/14",
"    keyword \\\\begin{itemize} brightred/14",
"    keyword \\\\begin{enumerate} brightred/14",
"    keyword \\\\begin{description} brightred/14",
"    keyword \\\\begin{list} brightred/14",
"    keyword \\\\begin{array} brightred/14",
"    keyword \\\\begin{tabular} brightred/14",
"    keyword \\\\begin{thebibliography}{*} brightred/14",
"    keyword \\\\begin{sloppypar} brightred/14",
"",
"    keyword \\\\end{document} brightred/14",
"    keyword \\\\end{equation} brightred/14",
"    keyword \\\\end{eqnarray} brightred/14",
"    keyword \\\\end{quote} brightred/14",
"    keyword \\\\end{quotation} brightred/14",
"    keyword \\\\end{center} brightred/14",
"    keyword \\\\end{verse} brightred/14",
"    keyword \\\\end{verbatim} brightred/14",
"    keyword \\\\end{itemize} brightred/14",
"    keyword \\\\end{enumerate} brightred/14",
"    keyword \\\\end{description} brightred/14",
"    keyword \\\\end{list} brightred/14",
"    keyword \\\\end{array} brightred/14",
"    keyword \\\\end{tabular} brightred/14",
"    keyword \\\\end{thebibliography}{*} brightred/14",
"    keyword \\\\end{sloppypar} brightred/14",
"",
"    keyword \\\\begin{*} brightcyan/16",
"    keyword \\\\end{*} brightcyan/16",
"",
"    keyword \\\\theorem{*}{*} yellow/24",
"",
"# if all else fails",
"    keyword whole \\\\+[*]{*}{*}{*} brightcyan/17",
"    keyword whole \\\\+[*]{*}{*} brightcyan/17",
"    keyword whole \\\\+{*}{*}{*}{*} brightcyan/17",
"    keyword whole \\\\+{*}{*}{*} brightcyan/17",
"    keyword whole \\\\+{*}{*} brightcyan/17",
"    keyword whole \\\\+{*} brightcyan/17",
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\n brightcyan/17",
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\s brightcyan/17",
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\t brightcyan/17",
"",
"context \\\\pagenumbering{ } yellow/20",
"    keyword arabic brightcyan/17",
"    keyword roman brightcyan/17",
"    keyword alph brightcyan/17",
"    keyword Roman brightcyan/17",
"    keyword Alph brightcyan/17",
"",
"context % \\n brown/22",
"",
"# mathematical formulas",
"context $ $ brightgreen/6",
"context exclusive \\\\begin{equation} \\\\end{equation} brightgreen/6",
"context exclusive \\\\begin{eqnarray} \\\\end{eqnarray} brightgreen/6",
"",
"",
"###############################################################################",
"file ..\\*\\\\.([chC]|CC|cxx|cc|cpp|CPP|CXX)$ C/C\\+\\+\\sProgram",
"context default",
"    keyword whole auto yellow/24",
"    keyword whole break yellow/24",
"    keyword whole case yellow/24",
"    keyword whole char yellow/24",
"    keyword whole const yellow/24",
"    keyword whole continue yellow/24",
"    keyword whole default yellow/24",
"    keyword whole do yellow/24",
"    keyword whole double yellow/24",
"    keyword whole else yellow/24",
"    keyword whole enum yellow/24",
"    keyword whole extern yellow/24",
"    keyword whole float yellow/24",
"    keyword whole for yellow/24",
"    keyword whole goto yellow/24",
"    keyword whole if yellow/24",
"    keyword whole int yellow/24",
"    keyword whole long yellow/24",
"    keyword whole register yellow/24",
"    keyword whole return yellow/24",
"    keyword whole short yellow/24",
"    keyword whole signed yellow/24",
"    keyword whole sizeof yellow/24",
"    keyword whole static yellow/24",
"    keyword whole struct yellow/24",
"    keyword whole switch yellow/24",
"    keyword whole typedef yellow/24",
"    keyword whole union yellow/24",
"    keyword whole unsigned yellow/24",
"    keyword whole void yellow/24",
"    keyword whole volatile yellow/24",
"    keyword whole while yellow/24",
"    keyword whole asm yellow/24",
"    keyword whole catch yellow/24",
"    keyword whole class yellow/24",
"    keyword whole friend yellow/24",
"    keyword whole delete yellow/24",
"    keyword whole inline yellow/24",
"    keyword whole new yellow/24",
"    keyword whole operator yellow/24",
"    keyword whole private yellow/24",
"    keyword whole protected yellow/24",
"    keyword whole public yellow/24",
"    keyword whole this yellow/24",
"    keyword whole throw yellow/24",
"    keyword whole template yellow/24",
"    keyword whole try yellow/24",
"    keyword whole virtual yellow/24",
"    keyword whole bool yellow/24",
"    keyword whole const_cast yellow/24",
"    keyword whole dynamic_cast yellow/24",
"    keyword whole explicit yellow/24",
"    keyword whole false yellow/24",
"    keyword whole mutable yellow/24",
"    keyword whole namespace yellow/24",
"    keyword whole reinterpret_cast yellow/24",
"    keyword whole static_cast yellow/24",
"    keyword whole true yellow/24",
"    keyword whole typeid yellow/24",
"    keyword whole typename yellow/24",
"    keyword whole using yellow/24",
"    keyword whole wchar_t yellow/24",
"    keyword whole ... yellow/24",
"",
"    keyword /\\* brown/22",
"    keyword \\*/ brown/22",
"",
"    keyword '\\s' brightgreen/16",
"    keyword '+' brightgreen/16",
"    keyword > yellow/24",
"    keyword < yellow/24",
"    keyword \\+ yellow/24",
"    keyword - yellow/24",
"    keyword \\* yellow/24",
"#    keyword / yellow/24",
"    keyword % yellow/24",
"    keyword = yellow/24",
"    keyword != yellow/24",
"    keyword == yellow/24",
"    keyword { brightcyan/14",
"    keyword } brightcyan/14",
"    keyword ( brightcyan/15",
"    keyword ) brightcyan/15",
"    keyword [ brightcyan/14",
"    keyword ] brightcyan/14",
"    keyword , brightcyan/14",
"    keyword : brightcyan/14",
"    keyword ; brightmagenta/19",
"context exclusive /\\* \\*/ brown/22",
"context // \\n brown/22",
"context linestart # \\n brightred/18",
"    keyword \\\\\\n yellow/24",
"    keyword /\\**\\*/ brown/22",
"    keyword \"+\" red/19",
"    keyword <+> red/19",
"context \" \" green/6",
"    keyword \\\\\" brightgreen/16",
"    keyword %% brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16",
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16",
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16",
"    keyword %\\[hl\\]n brightgreen/16",
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16",
"    keyword %[*] brightgreen/16",
"    keyword %c brightgreen/16",
"    keyword \\\\\\\\ brightgreen/16",
"    keyword \\\\' brightgreen/16",
"    keyword \\\\a brightgreen/16",
"    keyword \\\\b brightgreen/16",
"    keyword \\\\t brightgreen/16",
"    keyword \\\\n brightgreen/16",
"    keyword \\\\v brightgreen/16",
"    keyword \\\\f brightgreen/16",
"    keyword \\\\r brightgreen/16",
"    keyword \\\\0 brightgreen/16",
"",
"###############################################################################",
"file .\\*ChangeLog$ GNU\\sDistribution\\sChangeLog\\sFile",
"",
"context default",
"    keyword \\s+() brightmagenta/23",
"    keyword \\t+() brightmagenta/23",
"",
"context linestart \\t\\* : brightcyan/17",
"context linestart \\s\\s\\s\\s\\s\\s\\s\\s\\* : brightcyan/17",
"",
"context linestart 19+-+\\s \\n            yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart 20+-+\\s \\n            yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Mon\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Tue\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Wed\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Thu\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Fri\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Sat\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"context linestart Sun\\s+\\s+\\s+\\s \\n     yellow/24",
"    keyword <+@+> 			brightred/19",
"",
"",
"###############################################################################",
"file .\\*Makefile[\\\\\\.a-z]\\*$ Makefile",
"",
"context default",
"    keyword $(*) yellow/24",
"    keyword ${*} brightgreen/16",
"    keyword whole linestart include magenta",
"    keyword whole linestart endif magenta",
"    keyword whole linestart ifeq magenta",
"    keyword whole linestart ifneq magenta",
"    keyword whole linestart else magenta",
"    keyword linestart \\t lightgray/13 red",
"    keyword whole .PHONY white/25",
"    keyword whole .NOEXPORT white/25",
"    keyword = white/25",
"    keyword : yellow/24",
"    keyword \\\\\\n yellow/24",
"# this handles strange cases like @something@@somethingelse@ properly",
"    keyword whole @+@ brightmagenta/23 black/0",
"    keyword @+@ brightmagenta/23 black/0",
"",
"context linestart # \\n brown/22",
"    keyword whole @+@ brightmagenta/23 black/0",
"    keyword @+@ brightmagenta/23 black/0",
"",
"context exclusive = \\n brightcyan/17",
"    keyword \\\\\\n yellow/24",
"    keyword $(*) yellow/24",
"    keyword ${*} brightgreen/16",
"    keyword linestart \\t lightgray/13 red",
"    keyword whole @+@ brightmagenta/23 black/0",
"    keyword @+@ brightmagenta/23 black/0",
"",
"context exclusive linestart \\t \\n",
"    keyword \\\\\\n yellow/24",
"    keyword $(*) yellow/24",
"    keyword ${*} brightgreen/16",
"    keyword linestart \\t lightgray/13 red",
"    keyword whole @+@ brightmagenta/23 black/0",
"    keyword @+@ brightmagenta/23 black/0",
"",
"###############################################################################",
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
"file \\.\\* Help\\ssupport\\sother\\sfile\\stypes",
"context default",
"file \\.\\* by\\scoding\\srules\\sin\\s~/.cedit/syntax.",
"context default",
"file \\.\\* See\\sman/syntax\\sin\\sthe\\ssource\\sdistribution",
"context default",
"file \\.\\* and\\sconsult\\sthe\\sman\\spage.",
"context default",
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
		    if (line_error)
			result = line + line_error;
		    else {
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
	sprintf (s, _ (" Syntax error in file %s on line %d "), f, r);
	edit_error_dialog (_ (" Load syntax file "), s);
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

