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
#ifdef MIDNIGHT
#include "edit.h"
#else
#include "coolwidget.h"
#endif

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
    int c;
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
	if (*p == '\001') {
	    p++;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == '\n')
		    return 0;
		if (c == *p)
		    break;
		i++;
	    }
	} else {
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
    int c;
    if (!*text)
	return 0;
    if (whole_right)
	if (strchr (whole_right, edit_get_byte (edit, i + 1)))
	    return 0;
    for (p = text + strlen (text) - 1; (unsigned long) p >= (unsigned long) text; p--, i--) {
	if (*p == '\001') {
	    p--;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == '\n')
		    return 0;
		if (c == *p)
		    break;
		i--;
	    }
	} else {
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
#define debug_printf(x,y) printf(x,y)
#else
#define debug_printf(x,y)
#endif

static unsigned long apply_rules_going_right (WEdit * edit, long i, unsigned long rule)
{
    struct context_rule *r;
    int context, keyword, c1, c2;
    unsigned long border;
    context = (rule & RULE_CONTEXT) >> RULE_CONTEXT_SHIFT;
    keyword = (rule & RULE_WORD) >> RULE_WORD_SHIFT;
    border = rule & (RULE_ON_LEFT_BORDER | RULE_ON_RIGHT_BORDER);
    c1 = edit_get_byte (edit, i - 1);
    c2 = edit_get_byte (edit, i);

    debug_printf ("%c->", c1);
    debug_printf ("%c ", c2);
/* check to turn off a context */
    if (context && !keyword) {
	r = edit->rules[context];
	if (r->first_right == c2 && compare_word_to_right (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
	    debug_printf ("3 ", 0);
	    border = RULE_ON_RIGHT_BORDER;
	    if (r->between_delimiters) {
		context = 0;
		debug_printf ("context=off ", 0);
		keyword = 0;
	    }
	} else if (!r->between_delimiters && r->last_right == c1 && compare_word_to_left (edit, i - 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
	    debug_printf ("4 ", 0);
	    border = 0;
	    if (!(rule & RULE_ON_LEFT_BORDER)) {
		context = 0;
		debug_printf ("context=off ", 0);
		keyword = 0;
	    }
	} else if (r->last_left == c1 && compare_word_to_left (edit, i - 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
	    debug_printf ("2 ", 0);
	    border = 0;
	    keyword = 0;
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");

    debug_printf ("\n", 0);

/* check to turn off a keyword */
    if (keyword) {
	struct key_word *k;
	k = edit->rules[context]->keyword[keyword];
	if (k->last == c1 && compare_word_to_left (edit, i - 1, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
	    keyword = 0;
	    debug_printf ("keyword=%d ", keyword);
	}
    }
/* check to turn on a context */
    if (!context && !keyword) {
	int count;
	for (count = 1; edit->rules[count]; count++) {
	    r = edit->rules[count];
	    if (r->between_delimiters && r->last_right == c1 && compare_word_to_left (edit, i - 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
		debug_printf ("4 count=%d", count);
		border = 0;
		break;
	    } else if (r->first_left == c2 && compare_word_to_right (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
		debug_printf ("1 ", 0);
		border = RULE_ON_LEFT_BORDER;
		if (!r->between_delimiters) {
		    context = count;
		    debug_printf ("context=%d ", context);
		    keyword = 0;
		}
		break;
	    } else if (r->between_delimiters && r->last_left == c1 && compare_word_to_left (edit, i - 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
		debug_printf ("2 ", 0);
		border = 0;
		if (!(rule & RULE_ON_RIGHT_BORDER)) {
		    if (r->between_delimiters) {
			context = count;
			debug_printf ("context=%d ", context);
			keyword = 0;
		    }
		    break;
		}
	    }
	}
    }
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
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");
    debug_printf ("keyword=%d ", keyword);

    debug_printf (" %d#\n", context);

    return (context << RULE_CONTEXT_SHIFT) | (keyword << RULE_WORD_SHIFT) | border;
}

static inline int resolve_left_delim (WEdit * edit, long i, struct context_rule *r, int s)
{
    int c, count;
    if (!r->conflicts)
	return s;
    for (;;) {
	c = edit_get_byte (edit, i);
	if (c == '\n')
	    break;
	for (count = 1; r->conflicts[count]; count++) {
	    struct context_rule *p;
	    p = edit->rules[r->conflicts[count]];
	    if (!p)
		break;
	    if (p->first_left == c && r->between_delimiters == p->between_delimiters && compare_word_to_right (edit, i, p->left, p->whole_word_chars_left, r->whole_word_chars_right, p->line_start_left))
		return r->conflicts[count];
	}
	i--;
    }
    return 0;
}

/* we don't concern ourselves with single words here, 'cos we will always
   start at the beginning of a line and then go right */
static unsigned long apply_rules_going_left (WEdit * edit, long i, unsigned long rule)
{
    struct context_rule *r;
    int context, keyword, c1, c2;
    unsigned long border;
    context = (rule & RULE_CONTEXT) >> RULE_CONTEXT_SHIFT;
    keyword = (rule & RULE_WORD) >> RULE_WORD_SHIFT;
    border = rule & (RULE_ON_LEFT_BORDER | RULE_ON_RIGHT_BORDER);
    c1 = edit_get_byte (edit, i);
    c2 = edit_get_byte (edit, i + 1);

    debug_printf ("%c<-", c1);
    debug_printf ("%c ", c2);

/* check to turn off a context */
    if (context && !keyword) {
	r = edit->rules[context];
	if (r->first_left == c2 && compare_word_to_right (edit, i + 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
	    debug_printf ("1 ", 0);
	    border = 0;
	    if (!(rule & RULE_ON_RIGHT_BORDER)) {
		context = 0;
		keyword = 0;
		debug_printf ("context=off ", 0);
	    }
	} else if (!r->between_delimiters && r->first_right == c2 && compare_word_to_right (edit, i + 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
	    debug_printf ("3 ", 0);
	    border = 0;
	    keyword = 0;
	} else if (r->last_left == c1 && compare_word_to_left (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
	    debug_printf ("2 ", 0);
	    border = RULE_ON_LEFT_BORDER;
	    if (r->between_delimiters) {
		context = 0;
		keyword = 0;
		debug_printf ("context=off ", 0);
	    }
	}
    }
/* check to turn off a keyword */
    if (keyword) {
	struct key_word *k;
	k = edit->rules[context]->keyword[keyword];
	if (k->first == c2)
	    if (compare_word_to_right (edit, i + 1, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
		keyword = 0;
		debug_printf ("keyword=%d ", keyword);
	    }
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");

    debug_printf ("\n", 0);

/* check to turn on a context */
    if (!context && !keyword) {

	int count;
	for (count = 1; edit->rules[count]; count++) {
	    r = edit->rules[count];
	    if (r->first_left == c2 && compare_word_to_right (edit, i + 1, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
		debug_printf ("1 ", 0);
		border = 0;
		keyword = 0;
		break;
	    } else if (r->last_right == c1 && compare_word_to_left (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
		debug_printf ("4 count=%d", count);
		if (!r->between_delimiters && !(c1 == '\n' && r->single_char)) {
		    border = RULE_ON_RIGHT_BORDER;
		    context = resolve_left_delim (edit, i - 1, r, count);
		    debug_printf ("context=%d ", context);
		}
		break;
	    } else if (r->between_delimiters && r->first_right == c2 && compare_word_to_right (edit, i + 1, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
		debug_printf ("3 ", 0);
		border = 0;
		if (!(rule & RULE_ON_LEFT_BORDER))
		    if (r->between_delimiters && !(c2 == '\n' && r->single_char)) {
			context = resolve_left_delim (edit, i, r, count);
			keyword = 0;
			debug_printf ("context=%d ", context);
		    }
		break;
	    }
	}
    }
/* check to turn on a keyword */
    if (!keyword) {
	char *p;
	p = (r = edit->rules[context])->keyword_last_chars;
	while ((p = strchr (p + 1, c1))) {
	    struct key_word *k;
	    int count;
	    count = (unsigned long) p - (unsigned long) r->keyword_last_chars;
	    k = r->keyword[count];
	    if (k->last == c1 && compare_word_to_left (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
		keyword = count;
		debug_printf ("keyword=%d ", keyword);
		break;
	    }
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_LEFT_BORDER) ? "left" : "right") : "off");

    debug_printf (" %d#\n", context);

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
	for (i = edit->last_get_rule + 1; i <= byte_index; i++)
	    edit->rule = apply_rules_going_right (edit, i, edit->rule);
    } else if (byte_index < edit->last_get_rule) {
	for (i = edit->last_get_rule - 1; i >= byte_index; i--)
	    edit->rule = apply_rules_going_left (edit, i, edit->rule);
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
	*fg = NORMAL_COLOR;
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
    char *r, *p;
    p = r = strdup (s);
    while (*s) {
	switch (*s) {
	case '\\':
	    s++;
	    switch (*s) {
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
	    default:
		*p = *s;
		break;
	    }
	    break;
	case '*':
/* a * at the beginning or end of the line must be interpreted literally */
	    if ((unsigned long) p == (unsigned long) r || strlen (s) == 1)
		*p = '*';
	    else
		*p = '\001';
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
#endif

/* returns line number on error */
static int edit_read_syntax_rules (WEdit * edit, FILE * f)
{
    char *fg, *bg;
    char whole_right[256];
    char whole_left[256];
    char *args[1024], *l = 0;
    int line = 0;
    struct context_rule **r, *c;
    int num_words = -1, num_contexts = -1;
    int argc, result = 0;
    int i, j;

    args[0] = 0;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");

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
#ifdef MIDNIGHT
	    c->keyword[0]->fg = try_alloc_color_pair (fg, bg);
#else
	    c->keyword[0]->fg = allocate_color (fg);
	    c->keyword[0]->bg = allocate_color (bg);
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
#ifdef MIDNIGHT
	    k->fg = try_alloc_color_pair (fg, bg);
#else
	    k->fg = allocate_color (fg);
	    k->bg = allocate_color (bg);
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
    for (i = 1; edit->rules[i]; i++) {
	for (j = i + 1; edit->rules[j]; j++) {
	    if (strstr (edit->rules[j]->right, edit->rules[i]->right) && strcmp (edit->rules[i]->right, "\n")) {
		unsigned char *s;
		if (!edit->rules[i]->conflicts)
		    edit->rules[i]->conflicts = syntax_malloc (sizeof (unsigned char) * 260);
		s = edit->rules[i]->conflicts;
		s[strlen ((char *) s)] = (unsigned char) j;
	    }
	}
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
	syntax_free (edit->rules[i]->conflicts);
	syntax_free (edit->rules[i]->left);
	syntax_free (edit->rules[i]->right);
	syntax_free (edit->rules[i]->whole_word_chars_left);
	syntax_free (edit->rules[i]->whole_word_chars_right);
	syntax_free (edit->rules[i]->keyword);
	syntax_free (edit->rules[i]->keyword_first_chars);
	syntax_free (edit->rules[i]->keyword_last_chars);
	syntax_free (edit->rules[i]);
    }
    syntax_free (edit->rules);
}


#ifdef MIDNIGHT

char *syntax_text = 
"# Allowable colors for mc are\n"
"# black\n"
"# red\n"
"# green\n"
"# brown\n"
"# blue\n"
"# magenta\n"
"# cyan\n"
"# lightgray\n"
"# gray\n"
"# brightred\n"
"# brightgreen\n"
"# yellow\n"
"# brightblue\n"
"# brightmagenta\n"
"# brightcyan\n"
"# white\n"
"\n"
"file ..\\*\\\\.([chC]|CC|cxx|cc|cpp|CPP|CXX)$ C/C++\\sProgram\n"
"context default\n"
"    keyword whole void yellow\n"
"    keyword whole int yellow\n"
"    keyword whole unsigned yellow\n"
"    keyword whole char yellow\n"
"    keyword whole long yellow\n"
"    keyword whole if yellow\n"
"    keyword whole for yellow\n"
"    keyword whole while yellow\n"
"    keyword whole do yellow\n"
"    keyword whole else yellow\n"
"    keyword whole double yellow\n"
"    keyword whole switch yellow\n"
"    keyword whole case yellow\n"
"    keyword whole default yellow\n"
"    keyword whole static yellow\n"
"    keyword whole extern yellow\n"
"    keyword whole struct yellow\n"
"    keyword whole typedef yellow\n"
"    keyword whole ... yellow\n"
"    keyword whole inline yellow\n"
"    keyword whole return yellow\n"
"    keyword '*' yellow\n"
"    keyword > yellow\n"
"    keyword < yellow\n"
"    keyword + yellow\n"
"    keyword - yellow\n"
"    keyword \\* yellow\n"
"    keyword / yellow\n"
"    keyword % yellow\n"
"    keyword = yellow\n"
"    keyword != yellow\n"
"    keyword == yellow\n"
"    keyword { brightcyan\n"
"    keyword } brightcyan\n"
"    keyword ( brightcyan\n"
"    keyword ) brightcyan\n"
"    keyword [ brightcyan\n"
"    keyword ] brightcyan\n"
"    keyword , brightcyan\n"
"    keyword : brightcyan\n"
"    keyword ; brightmagenta\n"
"context /\\* \\*/ brown\n"
"context linestart # \\n brightred\n"
"    keyword \\\\\\n yellow\n"
"    keyword /\\**\\*/ brown\n"
"    keyword \"*\" red\n"
"    keyword <*> red\n"
"context \" \" green\n"
"    keyword %d yellow\n"
"    keyword %s yellow\n"
"    keyword %c yellow\n"
"    keyword %lu yellow\n"
"    keyword \\\\\" yellow\n";

#else

char *syntax_text = 
"file ..\\*\\\\.([chC]|CC|cxx|cc|cpp|CPP|CXX)$ C/C++\\sProgram\n"
"context default\n"
"    keyword whole void 24\n"
"    keyword whole int 24\n"
"    keyword whole unsigned 24\n"
"    keyword whole char 24\n"
"    keyword whole long 24\n"
"    keyword whole if 24\n"
"    keyword whole for 24\n"
"    keyword whole while 24\n"
"    keyword whole do 24\n"
"    keyword whole else 24\n"
"    keyword whole double 24\n"
"    keyword whole switch 24\n"
"    keyword whole case 24\n"
"    keyword whole default 24\n"
"    keyword whole static 24\n"
"    keyword whole extern 24\n"
"    keyword whole struct 24\n"
"    keyword whole typedef 24\n"
"    keyword whole ... 24\n"
"    keyword whole inline 24\n"
"    keyword whole return 24\n"
"    keyword '*' 6\n"
"    keyword > 24\n"
"    keyword < 24\n"
"    keyword + 24\n"
"    keyword - 24\n"
"    keyword \\* 24\n"
"    keyword / 24\n"
"    keyword % 24\n"
"    keyword = 24\n"
"    keyword != 24\n"
"    keyword == 24\n"
"    keyword { 14\n"
"    keyword } 14\n"
"    keyword ( 15\n"
"    keyword ) 15\n"
"    keyword [ 14\n"
"    keyword ] 14\n"
"    keyword , 14\n"
"    keyword : 14\n"
"    keyword ; 19\n"
"context /\\* \\*/ 22\n"
"context linestart # \\n 18\n"
"    keyword \\\\\\n 24\n"
"    keyword /\\**\\*/ 22\n"
"    keyword \"*\" 19\n"
"    keyword <*> 19\n"
"context \" \" 6\n"
"    keyword %d 24\n"
"    keyword %s 24\n"
"    keyword %c 24\n"
"    keyword %lu 24\n"
"    keyword \\\\\" 24\n"
"file \\.\\* Help\\ssupport\\sother\\sfile\\stypes\n"
"context default\n"
"file \\.\\* by\\scoding\\srules\\sin\\s~/.cedit/syntax.\n"
"context default\n"
"file \\.\\* See\\sman/syntax\\sin\\sthe\\ssource\\sdistribution\n"
"context default\n"
"file \\.\\* and\\sconsult\\sthe\\sman\\spage.\n"
"context default\n";

#endif

/* returns -1 on file error, line number on error in file syntax */
static int edit_read_syntax_file (WEdit * edit, char **names, char *syntax_file, char *editor_file, char *type)
{
    FILE *f;
    regex_t r;
    regmatch_t pmatch[1];
    char *args[1024], *l;
    int line = 0;
    int argc;
    int result = 0;
    int count = 0;

    f = fopen (syntax_file, "r");
    if (!f) {
	f = fopen (syntax_file, "w");
	if (!f)
	    return -1;
	fprintf (f, "%s", syntax_text);
	fclose (f);
	f = fopen (syntax_file, "r");
	if (!f)
	    return -1;
    }
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
	    if (names) {
		names[count++] = strdup (args[2]);
		names[count] = 0;
	    } else if (type) {
		if (!strcmp (type, args[2]))
		    goto found_type;
	    } else if (editor_file && edit) {
		if (!regexec (&r, editor_file, 1, pmatch, 0)) {
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

/* loads rules into edit struct. one of edit or names must be zero. if
   edit is zero, a list of types will be stored into name. type may be zero
   in which case the type will be selected according to the filename. */
void edit_load_syntax (WEdit * edit, char **names, char *type)
{
    int r;
    char *f;

    edit_free_syntax_rules (edit);

#ifdef MIDNIGHT
    if (!SLtt_Use_Ansi_Colors)
	return;
#endif

    if (edit) {
	if (!edit->filename)
	    return;
	if (!*edit->filename)
	    return;
    }
    f = catstrs (home_dir, SYNTAX_FILE, 0);
    r = edit_read_syntax_file (edit, names, f, edit ? edit->filename : 0, type);
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


