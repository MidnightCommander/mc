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

static inline unsigned long apply_rules_going_left (WEdit * edit, long i, unsigned long rule)
{
    struct context_rule *r;
    int context, contextchanged = 0, keyword, c2, c1;
    int found_left = 0, found_right = 0, keyword_foundright = 0;
    int done = 0;
    unsigned long border;
    context = (rule & RULE_CONTEXT) >> RULE_CONTEXT_SHIFT;
    keyword = (rule & RULE_WORD) >> RULE_WORD_SHIFT;
    border = rule & (RULE_ON_RIGHT_BORDER | RULE_ON_LEFT_BORDER);
    c1 = edit_get_byte (edit, i);
    c2 = edit_get_byte (edit, i + 1);
    if (!c2 || !c1)
	return rule;

    debug_printf ("%c->", c2);
    debug_printf ("%c ", c1);

/* check to turn off a keyword */
    if (keyword) {
	struct key_word *k;
	k = edit->rules[context]->keyword[keyword];
	if (c2 == '\n')
	    keyword = 0;
	if ((k->first == c2 && compare_word_to_right (edit, i + 1, k->keyword, k->whole_word_chars_right, k->whole_word_chars_left, k->line_start)) || (c2 == '\n')) {
	    keyword = 0;
	    keyword_foundright = 1;
	    debug_printf ("keyword=%d ", keyword);
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_RIGHT_BORDER) ? "right" : "left") : "off");

/* check to turn off a context */
    if (context && !keyword) {
	r = edit->rules[context];
	if (r->last_left == c1 && compare_word_to_left (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left) \
	    &&!(rule & RULE_ON_LEFT_BORDER)) {
	    debug_printf ("A:2 ", 0);
	    found_left = 1;
	    border = RULE_ON_LEFT_BORDER;
	    if (r->between_delimiters)
		context = 0;
	} else if (!found_right) {
	    if (r->first_left == c2 && compare_word_to_right (edit, i + 1, r->left, r->whole_word_chars_right, r->whole_word_chars_left, r->line_start_left) \
		&&(rule & RULE_ON_LEFT_BORDER)) {
/* always turn off a context at 4 */
		debug_printf ("A:1 ", 0);
		found_right = 1;
		border = 0;
		if (!keyword_foundright)
		    context = 0;
	    } else if (r->first_right == c2 && compare_word_to_right (edit, i + 1, r->right, r->whole_word_chars_right, r->whole_word_chars_left, r->line_start_right) \
		       &&(rule & RULE_ON_RIGHT_BORDER)) {
/* never turn off a context at 2 */
		debug_printf ("A:3 ", 0);
		found_right = 1;
		border = 0;
	    }
	}
    }
    debug_printf ("\n", 0);

/* check to turn on a keyword */
    if (!keyword) {
	char *p;
	p = (r = edit->rules[context])->keyword_last_chars;
	while ((p = strchr (p + 1, c1))) {
	    struct key_word *k;
	    int count;
	    count = (unsigned long) p - (unsigned long) r->keyword_last_chars;
	    k = r->keyword[count];
	    if (compare_word_to_left (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
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
	    if (!found_right) {
		if (r->first_left == c2 && compare_word_to_right (edit, i + 1, r->left, r->whole_word_chars_right, r->whole_word_chars_left, r->line_start_left) \
		    &&(rule & RULE_ON_LEFT_BORDER)) {
		    debug_printf ("B:1 count=%d", count);
		    found_right = 1;
		    border = 0;
		    context = 0;
		    contextchanged = 1;
		    keyword = 0;
		} else if (r->first_right == c2 && compare_word_to_right (edit, i + 1, r->right, r->whole_word_chars_right, r->whole_word_chars_left, r->line_start_right) \
			   &&(rule & RULE_ON_RIGHT_BORDER)) {
		    if (!(c2 == '\n' && r->single_char)) {
			debug_printf ("B:3 ", 0);
			found_right = 1;
			border = 0;
			if (r->between_delimiters) {
			    debug_printf ("context=%d ", context);
			    context = resolve_left_delim (edit, i, r, count);
			    contextchanged = 1;
			    keyword = 0;
			    if (r->last_left == c1 && compare_word_to_left (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left)) {
				debug_printf ("B:2 ", 0);
				found_left = 1;
				border = RULE_ON_LEFT_BORDER;
				context = 0;
			    }
			}
			break;
		    }
		}
	    }
	    if (!found_left) {
		if (r->last_right == c1 && compare_word_to_left (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) {
		    if (!(c1 == '\n' && r->single_char)) {
			debug_printf ("B:4 ", 0);
			found_left = 1;
			border = RULE_ON_RIGHT_BORDER;
			if (!keyword)
			    if (!r->between_delimiters)
				context = resolve_left_delim (edit, i - 1, r, count);
			break;
		    }
		}
	    }
	}
    }
    if (!keyword && contextchanged) {
	char *p;
	p = (r = edit->rules[context])->keyword_last_chars;
	while ((p = strchr (p + 1, c1))) {
	    struct key_word *k;
	    int coutner;
	    coutner = (unsigned long) p - (unsigned long) r->keyword_last_chars;
	    k = r->keyword[coutner];
	    if (compare_word_to_left (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start)) {
		keyword = coutner;
		debug_printf ("keyword=%d ", keyword);
		break;
	    }
	}
    }
    debug_printf ("border=%s ", border ? ((border & RULE_ON_RIGHT_BORDER) ? "right" : "left") : "off");
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
#if 0
    if (byte_index < edit->last_get_rule_start_display) {
/* this is for optimisation */
	for (i = edit->last_get_rule_start_display - 1; i >= byte_index; i--)
	    edit->rule_start_display = apply_rules_going_left (edit, i, edit->rule_start_display);
	edit->last_get_rule_start_display = byte_index;
	edit->rule = edit->rule_start_display;
    } else
#endif
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

#define CURRENT_SYNTAX_RULES_VERSION "33"

char *syntax_text = 
"# syntax rules version " CURRENT_SYNTAX_RULES_VERSION "\n"
"# (after the slash is a Cooledit color, 0-26 or any of the X colors in rgb.txt)\n"
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
"###############################################################################\n"
"file ..\\*\\\\.diff$ Unified\\sDiff\\sOutput ^diff.\\*(-u|--unified)\n"
"# yawn\n"
"context default\n"
"    keyword linestart @@*@@	green/16\n"
"    keyword linestart \\s black/0 white/26\n"
"context linestart diff \\n white/26 red/9\n"
"context linestart --- \\n brightmagenta/20\n"
"context linestart +++ \\n brightmagenta/20\n"
"context linestart + \\n brightgreen/6\n"
"context linestart - \\n brightred/18\n"
"context linestart A \\n white/26 black/0\n"
"context linestart B \\n white/26 black/0\n"
"context linestart C \\n white/26 black/0\n"
"context linestart D \\n white/26 black/0\n"
"context linestart E \\n white/26 black/0\n"
"context linestart F \\n white/26 black/0\n"
"context linestart G \\n white/26 black/0\n"
"context linestart H \\n white/26 black/0\n"
"context linestart I \\n white/26 black/0\n"
"context linestart J \\n white/26 black/0\n"
"context linestart K \\n white/26 black/0\n"
"context linestart L \\n white/26 black/0\n"
"context linestart M \\n white/26 black/0\n"
"context linestart N \\n white/26 black/0\n"
"context linestart O \\n white/26 black/0\n"
"context linestart P \\n white/26 black/0\n"
"context linestart Q \\n white/26 black/0\n"
"context linestart R \\n white/26 black/0\n"
"context linestart S \\n white/26 black/0\n"
"context linestart T \\n white/26 black/0\n"
"context linestart U \\n white/26 black/0\n"
"context linestart V \\n white/26 black/0\n"
"context linestart W \\n white/26 black/0\n"
"context linestart X \\n white/26 black/0\n"
"context linestart Y \\n white/26 black/0\n"
"context linestart Z \\n white/26 black/0\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.lsm$ LSM\\sFile\n"
"\n"
"context default\n"
"    keyword linestart Begin3		brightmagenta/20\n"
"    keyword linestart Title:\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Version:\\s\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Entered-date:\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Description:\\s\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Keywords:\\s\\s\\s\\s\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Alternate-site:\\s	red/9  yellow/24\n"
"    keyword linestart Primary-site:\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Original-site:\\s\\s	red/9  yellow/24\n"
"    keyword linestart Platforms:\\s\\s\\s\\s\\s\\s	red/9  yellow/24\n"
"    keyword linestart Copying-policy:\\s	red/9  yellow/24\n"
"    keyword linestart End			brightmagenta/20\n"
"\n"
"    keyword linestart \\t\\t				white/26 yellow/24\n"
"    keyword linestart \\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s\\s	white/26 yellow/24\n"
"    keyword whole GPL	green/6\n"
"    keyword whole BSD	green/6\n"
"    keyword whole Shareware	green/6\n"
"    keyword whole sunsite.unc.edu	green/6\n"
"    keyword wholeright \\s*.tar.gz	green/6\n"
"    keyword wholeright \\s*.lsm	green/6\n"
"\n"
"context linestart Author:\\s\\s\\s\\s\\s\\s\\s\\s\\s \\n brightred/19\n"
"    keyword whole \\s*@*\\s(*)		cyan/16\n"
"\n"
"context linestart Maintained-by:\\s\\s \\n brightred/19\n"
"    keyword whole \\s*@*\\s(*)		cyan/16\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.sh$ Shell\\sScript ^#!/.\\*/(ksh|bash|sh|pdkzsh)$\n"
"\n"
"context default\n"
"    keyword whole for yellow/24\n"
"    keyword whole in yellow/24\n"
"    keyword whole do yellow/24\n"
"    keyword whole done yellow/24\n"
"    keyword whole select yellow/24\n"
"    keyword whole case yellow/24\n"
"    keyword whole if yellow/24\n"
"    keyword whole then yellow/24\n"
"    keyword whole elif yellow/24\n"
"    keyword whole else yellow/24\n"
"    keyword whole fi yellow/24\n"
"    keyword whole while yellow/24\n"
"    keyword whole until yellow/24\n"
"    keyword ;; brightred/18\n"
"    keyword ` brightred/18\n"
"    keyword '*' green/6\n"
"    keyword \" green/6\n"
"    keyword ; brightcyan/17\n"
"    keyword $(*) brightgreen/16\n"
"    keyword ${*} brightgreen/16\n"
"    keyword { brightcyan/14\n"
"    keyword } brightcyan/14\n"
"\n"
"    keyword whole linestart #!/bin/\\[abkpct\\]sh brightcyan/17 black/0\n"
"\n"
"    keyword $\\* brightred/18\n"
"    keyword $@ brightred/18\n"
"    keyword $# brightred/18\n"
"    keyword $? brightred/18\n"
"    keyword $- brightred/18\n"
"    keyword $$ brightred/18\n"
"    keyword $! brightred/18\n"
"    keyword $_ brightred/18\n"
"\n"
"    keyword wholeright $\\[0123456789\\]0 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]1 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]2 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]3 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]4 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]5 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]6 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]7 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]8 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]9 brightred/18\n"
"\n"
"    keyword wholeright $+A brightgreen/16\n"
"    keyword wholeright $+B brightgreen/16\n"
"    keyword wholeright $+C brightgreen/16\n"
"    keyword wholeright $+D brightgreen/16\n"
"    keyword wholeright $+E brightgreen/16\n"
"    keyword wholeright $+F brightgreen/16\n"
"    keyword wholeright $+G brightgreen/16\n"
"    keyword wholeright $+H brightgreen/16\n"
"    keyword wholeright $+I brightgreen/16\n"
"    keyword wholeright $+J brightgreen/16\n"
"    keyword wholeright $+K brightgreen/16\n"
"    keyword wholeright $+L brightgreen/16\n"
"    keyword wholeright $+M brightgreen/16\n"
"    keyword wholeright $+N brightgreen/16\n"
"    keyword wholeright $+O brightgreen/16\n"
"    keyword wholeright $+P brightgreen/16\n"
"    keyword wholeright $+Q brightgreen/16\n"
"    keyword wholeright $+R brightgreen/16\n"
"    keyword wholeright $+S brightgreen/16\n"
"    keyword wholeright $+T brightgreen/16\n"
"    keyword wholeright $+U brightgreen/16\n"
"    keyword wholeright $+V brightgreen/16\n"
"    keyword wholeright $+W brightgreen/16\n"
"    keyword wholeright $+X brightgreen/16\n"
"    keyword wholeright $+Y brightgreen/16\n"
"    keyword wholeright $+Z brightgreen/16\n"
"\n"
"    keyword wholeright $+a brightgreen/16\n"
"    keyword wholeright $+b brightgreen/16\n"
"    keyword wholeright $+c brightgreen/16\n"
"    keyword wholeright $+d brightgreen/16\n"
"    keyword wholeright $+e brightgreen/16\n"
"    keyword wholeright $+f brightgreen/16\n"
"    keyword wholeright $+g brightgreen/16\n"
"    keyword wholeright $+h brightgreen/16\n"
"    keyword wholeright $+i brightgreen/16\n"
"    keyword wholeright $+j brightgreen/16\n"
"    keyword wholeright $+k brightgreen/16\n"
"    keyword wholeright $+l brightgreen/16\n"
"    keyword wholeright $+m brightgreen/16\n"
"    keyword wholeright $+n brightgreen/16\n"
"    keyword wholeright $+o brightgreen/16\n"
"    keyword wholeright $+p brightgreen/16\n"
"    keyword wholeright $+q brightgreen/16\n"
"    keyword wholeright $+r brightgreen/16\n"
"    keyword wholeright $+s brightgreen/16\n"
"    keyword wholeright $+t brightgreen/16\n"
"    keyword wholeright $+u brightgreen/16\n"
"    keyword wholeright $+v brightgreen/16\n"
"    keyword wholeright $+w brightgreen/16\n"
"    keyword wholeright $+x brightgreen/16\n"
"    keyword wholeright $+y brightgreen/16\n"
"    keyword wholeright $+z brightgreen/16\n"
"\n"
"    keyword wholeright $+0 brightgreen/16\n"
"    keyword wholeright $+1 brightgreen/16\n"
"    keyword wholeright $+2 brightgreen/16\n"
"    keyword wholeright $+3 brightgreen/16\n"
"    keyword wholeright $+4 brightgreen/16\n"
"    keyword wholeright $+5 brightgreen/16\n"
"    keyword wholeright $+6 brightgreen/16\n"
"    keyword wholeright $+7 brightgreen/16\n"
"    keyword wholeright $+8 brightgreen/16\n"
"    keyword wholeright $+9 brightgreen/16\n"
"\n"
"    keyword $ brightgreen/16\n"
"\n"
"    keyword wholeleft function*() brightblue/11\n"
"\n"
"    keyword whole arch cyan/14\n"
"    keyword whole ash cyan/14\n"
"    keyword whole awk cyan/14\n"
"    keyword whole basename cyan/14\n"
"    keyword whole bash cyan/14\n"
"    keyword whole basnemae cyan/14\n"
"    keyword whole bg_backup cyan/14\n"
"    keyword whole bg_restore cyan/14\n"
"    keyword whole bsh cyan/14\n"
"    keyword whole cat cyan/14\n"
"    keyword whole chgrp cyan/14\n"
"    keyword whole chmod cyan/14\n"
"    keyword whole chown cyan/14\n"
"    keyword whole cp cyan/14\n"
"    keyword whole cpio cyan/14\n"
"    keyword whole csh cyan/14\n"
"    keyword whole date cyan/14\n"
"    keyword whole dd cyan/14\n"
"    keyword whole df cyan/14\n"
"    keyword whole dmesg cyan/14\n"
"    keyword whole dnsdomainname cyan/14\n"
"    keyword whole doexec cyan/14\n"
"    keyword whole domainname cyan/14\n"
"    keyword whole echo cyan/14\n"
"    keyword whole ed cyan/14\n"
"    keyword whole egrep cyan/14\n"
"    keyword whole ex cyan/14\n"
"    keyword whole false cyan/14\n"
"    keyword whole fgrep cyan/14\n"
"    keyword whole fsconf cyan/14\n"
"    keyword whole gawk cyan/14\n"
"    keyword whole grep cyan/14\n"
"    keyword whole gunzip cyan/14\n"
"    keyword whole gzip cyan/14\n"
"    keyword whole hostname cyan/14\n"
"    keyword whole igawk cyan/14\n"
"    keyword whole ipcalc cyan/14\n"
"    keyword whole kill cyan/14\n"
"    keyword whole ksh cyan/14\n"
"    keyword whole linuxconf cyan/14\n"
"    keyword whole ln cyan/14\n"
"    keyword whole login cyan/14\n"
"    keyword whole lpdconf cyan/14\n"
"    keyword whole ls cyan/14\n"
"    keyword whole mail cyan/14\n"
"    keyword whole mkdir cyan/14\n"
"    keyword whole mknod cyan/14\n"
"    keyword whole mktemp cyan/14\n"
"    keyword whole more cyan/14\n"
"    keyword whole mount cyan/14\n"
"    keyword whole mt cyan/14\n"
"    keyword whole mv cyan/14\n"
"    keyword whole netconf cyan/14\n"
"    keyword whole netstat cyan/14\n"
"    keyword whole nice cyan/14\n"
"    keyword whole nisdomainname cyan/14\n"
"    keyword whole ping cyan/14\n"
"    keyword whole ps cyan/14\n"
"    keyword whole pwd cyan/14\n"
"    keyword whole red cyan/14\n"
"    keyword whole remadmin cyan/14\n"
"    keyword whole rm cyan/14\n"
"    keyword whole rmdir cyan/14\n"
"    keyword whole rpm cyan/14\n"
"    keyword whole sed cyan/14\n"
"    keyword whole setserial cyan/14\n"
"    keyword whole sh cyan/14\n"
"    keyword whole sleep cyan/14\n"
"    keyword whole sort cyan/14\n"
"    keyword whole stty cyan/14\n"
"    keyword whole su cyan/14\n"
"    keyword whole sync cyan/14\n"
"    keyword whole taper cyan/14\n"
"    keyword whole tar cyan/14\n"
"    keyword whole tcsh cyan/14\n"
"    keyword whole touch cyan/14\n"
"    keyword whole true cyan/14\n"
"    keyword whole umount cyan/14\n"
"    keyword whole uname cyan/14\n"
"    keyword whole userconf cyan/14\n"
"    keyword whole usleep cyan/14\n"
"    keyword whole vi cyan/14\n"
"    keyword whole view cyan/14\n"
"    keyword whole vim cyan/14\n"
"    keyword whole xconf cyan/14\n"
"    keyword whole ypdomainname cyan/14\n"
"    keyword whole zcat cyan/14\n"
"    keyword whole zsh cyan/14\n"
"\n"
"context # \\n brown/22\n"
"\n"
"context exclusive ` ` white/26 black/0\n"
"    keyword '*' green/6\n"
"    keyword \" green/6\n"
"    keyword ; brightcyan/17\n"
"    keyword $(*) brightgreen/16\n"
"    keyword ${*} brightgreen/16\n"
"    keyword { brightcyan/14\n"
"    keyword } brightcyan/14\n"
"\n"
"    keyword $\\* brightred/18\n"
"    keyword $@ brightred/18\n"
"    keyword $# brightred/18\n"
"    keyword $? brightred/18\n"
"    keyword $- brightred/18\n"
"    keyword $$ brightred/18\n"
"    keyword $! brightred/18\n"
"    keyword $_ brightred/18\n"
"\n"
"    keyword wholeright $\\[0123456789\\]0 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]1 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]2 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]3 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]4 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]5 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]6 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]7 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]8 brightred/18\n"
"    keyword wholeright $\\[0123456789\\]9 brightred/18\n"
"\n"
"    keyword wholeright $+A brightgreen/16\n"
"    keyword wholeright $+B brightgreen/16\n"
"    keyword wholeright $+C brightgreen/16\n"
"    keyword wholeright $+D brightgreen/16\n"
"    keyword wholeright $+E brightgreen/16\n"
"    keyword wholeright $+F brightgreen/16\n"
"    keyword wholeright $+G brightgreen/16\n"
"    keyword wholeright $+H brightgreen/16\n"
"    keyword wholeright $+I brightgreen/16\n"
"    keyword wholeright $+J brightgreen/16\n"
"    keyword wholeright $+K brightgreen/16\n"
"    keyword wholeright $+L brightgreen/16\n"
"    keyword wholeright $+M brightgreen/16\n"
"    keyword wholeright $+N brightgreen/16\n"
"    keyword wholeright $+O brightgreen/16\n"
"    keyword wholeright $+P brightgreen/16\n"
"    keyword wholeright $+Q brightgreen/16\n"
"    keyword wholeright $+R brightgreen/16\n"
"    keyword wholeright $+S brightgreen/16\n"
"    keyword wholeright $+T brightgreen/16\n"
"    keyword wholeright $+U brightgreen/16\n"
"    keyword wholeright $+V brightgreen/16\n"
"    keyword wholeright $+W brightgreen/16\n"
"    keyword wholeright $+X brightgreen/16\n"
"    keyword wholeright $+Y brightgreen/16\n"
"    keyword wholeright $+Z brightgreen/16\n"
"\n"
"    keyword wholeright $+a brightgreen/16\n"
"    keyword wholeright $+b brightgreen/16\n"
"    keyword wholeright $+c brightgreen/16\n"
"    keyword wholeright $+d brightgreen/16\n"
"    keyword wholeright $+e brightgreen/16\n"
"    keyword wholeright $+f brightgreen/16\n"
"    keyword wholeright $+g brightgreen/16\n"
"    keyword wholeright $+h brightgreen/16\n"
"    keyword wholeright $+i brightgreen/16\n"
"    keyword wholeright $+j brightgreen/16\n"
"    keyword wholeright $+k brightgreen/16\n"
"    keyword wholeright $+l brightgreen/16\n"
"    keyword wholeright $+m brightgreen/16\n"
"    keyword wholeright $+n brightgreen/16\n"
"    keyword wholeright $+o brightgreen/16\n"
"    keyword wholeright $+p brightgreen/16\n"
"    keyword wholeright $+q brightgreen/16\n"
"    keyword wholeright $+r brightgreen/16\n"
"    keyword wholeright $+s brightgreen/16\n"
"    keyword wholeright $+t brightgreen/16\n"
"    keyword wholeright $+u brightgreen/16\n"
"    keyword wholeright $+v brightgreen/16\n"
"    keyword wholeright $+w brightgreen/16\n"
"    keyword wholeright $+x brightgreen/16\n"
"    keyword wholeright $+y brightgreen/16\n"
"    keyword wholeright $+z brightgreen/16\n"
"\n"
"    keyword wholeright $+0 brightgreen/16\n"
"    keyword wholeright $+1 brightgreen/16\n"
"    keyword wholeright $+2 brightgreen/16\n"
"    keyword wholeright $+3 brightgreen/16\n"
"    keyword wholeright $+4 brightgreen/16\n"
"    keyword wholeright $+5 brightgreen/16\n"
"    keyword wholeright $+6 brightgreen/16\n"
"    keyword wholeright $+7 brightgreen/16\n"
"    keyword wholeright $+8 brightgreen/16\n"
"    keyword wholeright $+9 brightgreen/16\n"
"\n"
"    keyword $ brightgreen/16\n"
"\n"
"    keyword whole arch cyan/14\n"
"    keyword whole ash cyan/14\n"
"    keyword whole awk cyan/14\n"
"    keyword whole basename cyan/14\n"
"    keyword whole bash cyan/14\n"
"    keyword whole basnemae cyan/14\n"
"    keyword whole bg_backup cyan/14\n"
"    keyword whole bg_restore cyan/14\n"
"    keyword whole bsh cyan/14\n"
"    keyword whole cat cyan/14\n"
"    keyword whole chgrp cyan/14\n"
"    keyword whole chmod cyan/14\n"
"    keyword whole chown cyan/14\n"
"    keyword whole cp cyan/14\n"
"    keyword whole cpio cyan/14\n"
"    keyword whole csh cyan/14\n"
"    keyword whole date cyan/14\n"
"    keyword whole dd cyan/14\n"
"    keyword whole df cyan/14\n"
"    keyword whole dmesg cyan/14\n"
"    keyword whole dnsdomainname cyan/14\n"
"    keyword whole doexec cyan/14\n"
"    keyword whole domainname cyan/14\n"
"    keyword whole echo cyan/14\n"
"    keyword whole ed cyan/14\n"
"    keyword whole egrep cyan/14\n"
"    keyword whole ex cyan/14\n"
"    keyword whole false cyan/14\n"
"    keyword whole fgrep cyan/14\n"
"    keyword whole fsconf cyan/14\n"
"    keyword whole gawk cyan/14\n"
"    keyword whole grep cyan/14\n"
"    keyword whole gunzip cyan/14\n"
"    keyword whole gzip cyan/14\n"
"    keyword whole hostname cyan/14\n"
"    keyword whole igawk cyan/14\n"
"    keyword whole ipcalc cyan/14\n"
"    keyword whole kill cyan/14\n"
"    keyword whole ksh cyan/14\n"
"    keyword whole linuxconf cyan/14\n"
"    keyword whole ln cyan/14\n"
"    keyword whole login cyan/14\n"
"    keyword whole lpdconf cyan/14\n"
"    keyword whole ls cyan/14\n"
"    keyword whole mail cyan/14\n"
"    keyword whole mkdir cyan/14\n"
"    keyword whole mknod cyan/14\n"
"    keyword whole mktemp cyan/14\n"
"    keyword whole more cyan/14\n"
"    keyword whole mount cyan/14\n"
"    keyword whole mt cyan/14\n"
"    keyword whole mv cyan/14\n"
"    keyword whole netconf cyan/14\n"
"    keyword whole netstat cyan/14\n"
"    keyword whole nice cyan/14\n"
"    keyword whole nisdomainname cyan/14\n"
"    keyword whole ping cyan/14\n"
"    keyword whole ps cyan/14\n"
"    keyword whole pwd cyan/14\n"
"    keyword whole red cyan/14\n"
"    keyword whole remadmin cyan/14\n"
"    keyword whole rm cyan/14\n"
"    keyword whole rmdir cyan/14\n"
"    keyword whole rpm cyan/14\n"
"    keyword whole sed cyan/14\n"
"    keyword whole setserial cyan/14\n"
"    keyword whole sh cyan/14\n"
"    keyword whole sleep cyan/14\n"
"    keyword whole sort cyan/14\n"
"    keyword whole stty cyan/14\n"
"    keyword whole su cyan/14\n"
"    keyword whole sync cyan/14\n"
"    keyword whole taper cyan/14\n"
"    keyword whole tar cyan/14\n"
"    keyword whole tcsh cyan/14\n"
"    keyword whole touch cyan/14\n"
"    keyword whole true cyan/14\n"
"    keyword whole umount cyan/14\n"
"    keyword whole uname cyan/14\n"
"    keyword whole userconf cyan/14\n"
"    keyword whole usleep cyan/14\n"
"    keyword whole vi cyan/14\n"
"    keyword whole view cyan/14\n"
"    keyword whole vim cyan/14\n"
"    keyword whole xconf cyan/14\n"
"    keyword whole ypdomainname cyan/14\n"
"    keyword whole zcat cyan/14\n"
"    keyword whole zsh cyan/14\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.(pl|PL])$ Perl\\sProgram\\s(no\\srules\\syet) ^#!/.\\*/perl$\n"
"context default\n"
"    keyword whole linestart #!/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0\n"
"    keyword whole linestart #!/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0\n"
"    keyword whole linestart #!/\\[abcdefghijklmnopqrstuvwxyz\\]/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0\n"
"    keyword whole linestart #!/\\[abcdefghijklmnopqrstuvwxyz\\]/bin/perl brightcyan/17 black/0\n"
"    keyword whole linestart #!/bin/perl brightcyan/17 black/0\n"
"\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.(py|PY])$ Python\\sProgram ^#!/.\\*/python$\n"
"context default\n"
"    keyword whole : brightred/18\n"
"    keyword whole self brightred/18\n"
"    keyword whole access yellow/24\n"
"    keyword whole and yellow/24\n"
"    keyword whole break yellow/24\n"
"    keyword whole class yellow/24\n"
"    keyword whole continue yellow/24\n"
"    keyword whole def yellow/24\n"
"    keyword whole del yellow/24\n"
"    keyword whole elif yellow/24\n"
"    keyword whole else yellow/24\n"
"    keyword whole except yellow/24\n"
"    keyword whole exec yellow/24\n"
"    keyword whole finally yellow/24\n"
"    keyword whole for yellow/24\n"
"    keyword whole from yellow/24\n"
"    keyword whole global yellow/24\n"
"    keyword whole if yellow/24\n"
"    keyword whole import yellow/24\n"
"    keyword whole in yellow/24\n"
"    keyword whole is yellow/24\n"
"    keyword whole lambda yellow/24\n"
"    keyword whole not yellow/24\n"
"    keyword whole or yellow/24\n"
"    keyword whole pass yellow/24\n"
"    keyword whole print yellow/24\n"
"    keyword whole raise yellow/24\n"
"    keyword whole return yellow/24\n"
"    keyword whole try yellow/24\n"
"    keyword whole while yellow/24\n"
"\n"
"    keyword whole abs brightcyan/17\n"
"    keyword whole apply brightcyan/17\n"
"    keyword whole callable brightcyan/17\n"
"    keyword whole chr brightcyan/17\n"
"    keyword whole cmp brightcyan/17\n"
"    keyword whole coerce brightcyan/17\n"
"    keyword whole compile brightcyan/17\n"
"    keyword whole delattr brightcyan/17\n"
"    keyword whole dir brightcyan/17\n"
"    keyword whole divmod brightcyan/17\n"
"    keyword whole eval brightcyan/17\n"
"    keyword whole exec brightcyan/17\n"
"    keyword whole execfile brightcyan/17\n"
"    keyword whole filter brightcyan/17\n"
"    keyword whole float brightcyan/17\n"
"    keyword whole getattr brightcyan/17\n"
"    keyword whole globals brightcyan/17\n"
"    keyword whole hasattr brightcyan/17\n"
"    keyword whole hash brightcyan/17\n"
"    keyword whole hex brightcyan/17\n"
"    keyword whole id brightcyan/17\n"
"    keyword whole input brightcyan/17\n"
"    keyword whole int brightcyan/17\n"
"    keyword whole len brightcyan/17\n"
"    keyword whole locals brightcyan/17\n"
"    keyword whole long brightcyan/17\n"
"    keyword whole map brightcyan/17\n"
"    keyword whole max brightcyan/17\n"
"    keyword whole min brightcyan/17\n"
"    keyword whole oct brightcyan/17\n"
"    keyword whole open brightcyan/17\n"
"    keyword whole ord brightcyan/17\n"
"    keyword whole pow brightcyan/17\n"
"    keyword whole range brightcyan/17\n"
"    keyword whole raw_input brightcyan/17\n"
"    keyword whole reduce brightcyan/17\n"
"    keyword whole reload brightcyan/17\n"
"    keyword whole repr brightcyan/17\n"
"    keyword whole round brightcyan/17\n"
"    keyword whole setattr brightcyan/17\n"
"    keyword whole str brightcyan/17\n"
"    keyword whole tuple brightcyan/17\n"
"    keyword whole type brightcyan/17\n"
"    keyword whole vars brightcyan/17\n"
"    keyword whole xrange brightcyan/17\n"
"\n"
"    keyword whole atof magenta/23\n"
"    keyword whole atoi magenta/23\n"
"    keyword whole atol magenta/23\n"
"    keyword whole expandtabs magenta/23\n"
"    keyword whole find magenta/23\n"
"    keyword whole rfind magenta/23\n"
"    keyword whole index magenta/23\n"
"    keyword whole rindex magenta/23\n"
"    keyword whole count magenta/23\n"
"    keyword whole split magenta/23\n"
"    keyword whole splitfields magenta/23\n"
"    keyword whole join magenta/23\n"
"    keyword whole joinfields magenta/23\n"
"    keyword whole strip magenta/23\n"
"    keyword whole swapcase magenta/23\n"
"    keyword whole upper magenta/23\n"
"    keyword whole lower magenta/23\n"
"    keyword whole ljust magenta/23\n"
"    keyword whole rjust magenta/23\n"
"    keyword whole center magenta/23\n"
"    keyword whole zfill magenta/23\n"
"\n"
"    keyword whole __init__ lightgray/13\n"
"    keyword whole __del__ lightgray/13\n"
"    keyword whole __repr__ lightgray/13\n"
"    keyword whole __str__ lightgray/13\n"
"    keyword whole __cmp__ lightgray/13\n"
"    keyword whole __hash__ lightgray/13\n"
"    keyword whole __call__ lightgray/13\n"
"    keyword whole __getattr__ lightgray/13\n"
"    keyword whole __setattr__ lightgray/13\n"
"    keyword whole __delattr__ lightgray/13\n"
"    keyword whole __len__ lightgray/13\n"
"    keyword whole __getitem__ lightgray/13\n"
"    keyword whole __setitem__ lightgray/13\n"
"    keyword whole __delitem__ lightgray/13\n"
"    keyword whole __getslice__ lightgray/13\n"
"    keyword whole __setslice__ lightgray/13\n"
"    keyword whole __delslice__ lightgray/13\n"
"    keyword whole __add__ lightgray/13\n"
"    keyword whole __sub__ lightgray/13\n"
"    keyword whole __mul__ lightgray/13\n"
"    keyword whole __div__ lightgray/13\n"
"    keyword whole __mod__ lightgray/13\n"
"    keyword whole __divmod__ lightgray/13\n"
"    keyword whole __pow__ lightgray/13\n"
"    keyword whole __lshift__ lightgray/13\n"
"    keyword whole __rshift__ lightgray/13\n"
"    keyword whole __and__ lightgray/13\n"
"    keyword whole __xor__ lightgray/13\n"
"    keyword whole __or__ lightgray/13\n"
"    keyword whole __neg__ lightgray/13\n"
"    keyword whole __pos__ lightgray/13\n"
"    keyword whole __abs__ lightgray/13\n"
"    keyword whole __invert__ lightgray/13\n"
"    keyword whole __nonzero__ lightgray/13\n"
"    keyword whole __coerce__ lightgray/13\n"
"    keyword whole __int__ lightgray/13\n"
"    keyword whole __long__ lightgray/13\n"
"    keyword whole __float__ lightgray/13\n"
"    keyword whole __oct__ lightgray/13\n"
"    keyword whole __hex__ lightgray/13\n"
"\n"
"    keyword whole __init__ lightgray/13\n"
"    keyword whole __del__ lightgray/13\n"
"    keyword whole __repr__ lightgray/13\n"
"    keyword whole __str__ lightgray/13\n"
"    keyword whole __cmp__ lightgray/13\n"
"    keyword whole __hash__ lightgray/13\n"
"    keyword whole __call__ lightgray/13\n"
"    keyword whole __getattr__ lightgray/13\n"
"    keyword whole __setattr__ lightgray/13\n"
"    keyword whole __delattr__ lightgray/13\n"
"    keyword whole __len__ lightgray/13\n"
"    keyword whole __getitem__ lightgray/13\n"
"    keyword whole __setitem__ lightgray/13\n"
"    keyword whole __delitem__ lightgray/13\n"
"    keyword whole __getslice__ lightgray/13\n"
"    keyword whole __setslice__ lightgray/13\n"
"    keyword whole __delslice__ lightgray/13\n"
"    keyword whole __add__ lightgray/13\n"
"    keyword whole __sub__ lightgray/13\n"
"    keyword whole __mul__ lightgray/13\n"
"    keyword whole __div__ lightgray/13\n"
"    keyword whole __mod__ lightgray/13\n"
"    keyword whole __divmod__ lightgray/13\n"
"    keyword whole __pow__ lightgray/13\n"
"    keyword whole __lshift__ lightgray/13\n"
"    keyword whole __rshift__ lightgray/13\n"
"    keyword whole __and__ lightgray/13\n"
"    keyword whole __xor__ lightgray/13\n"
"    keyword whole __or__ lightgray/13\n"
"    keyword whole __neg__ lightgray/13\n"
"    keyword whole __pos__ lightgray/13\n"
"    keyword whole __abs__ lightgray/13\n"
"    keyword whole __invert__ lightgray/13\n"
"    keyword whole __nonzero__ lightgray/13\n"
"    keyword whole __coerce__ lightgray/13\n"
"    keyword whole __int__ lightgray/13\n"
"    keyword whole __long__ lightgray/13\n"
"    keyword whole __float__ lightgray/13\n"
"    keyword whole __oct__ lightgray/13\n"
"    keyword whole __hex__ lightgray/13\n"
"\n"
"    keyword whole __radd__ lightgray/13\n"
"    keyword whole __rsub__ lightgray/13\n"
"    keyword whole __rmul__ lightgray/13\n"
"    keyword whole __rdiv__ lightgray/13\n"
"    keyword whole __rmod__ lightgray/13\n"
"    keyword whole __rdivmod__ lightgray/13\n"
"    keyword whole __rpow__ lightgray/13\n"
"    keyword whole __rlshift__ lightgray/13\n"
"    keyword whole __rrshift__ lightgray/13\n"
"    keyword whole __rand__ lightgray/13\n"
"    keyword whole __rxor__ lightgray/13\n"
"    keyword whole __ror__ lightgray/13\n"
"\n"
"    keyword whole __*__ brightred/18\n"
"\n"
"context \"\"\" \"\"\" brown/22\n"
"context # \\n brown/22\n"
"context \" \" green/6\n"
"    keyword \\\\\" brightgreen/16\n"
"    keyword %% brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16\n"
"    keyword %\\[hl\\]n brightgreen/16\n"
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16\n"
"    keyword %[*] brightgreen/16\n"
"    keyword %c brightgreen/16\n"
"    keyword \\\\\\\\ brightgreen/16\n"
"    keyword \\\\' brightgreen/16\n"
"    keyword \\\\a brightgreen/16\n"
"    keyword \\\\b brightgreen/16\n"
"    keyword \\\\t brightgreen/16\n"
"    keyword \\\\n brightgreen/16\n"
"    keyword \\\\v brightgreen/16\n"
"    keyword \\\\f brightgreen/16\n"
"    keyword \\\\r brightgreen/16\n"
"    keyword \\\\0 brightgreen/16\n"
"\n"
"context ' ' green/6\n"
"    keyword \\\\\" brightgreen/16\n"
"    keyword %% brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16\n"
"    keyword %\\[hl\\]n brightgreen/16\n"
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16\n"
"    keyword %[*] brightgreen/16\n"
"    keyword %c brightgreen/16\n"
"    keyword \\\\\\\\ brightgreen/16\n"
"    keyword \\\\' brightgreen/16\n"
"    keyword \\\\a brightgreen/16\n"
"    keyword \\\\b brightgreen/16\n"
"    keyword \\\\t brightgreen/16\n"
"    keyword \\\\n brightgreen/16\n"
"    keyword \\\\v brightgreen/16\n"
"    keyword \\\\f brightgreen/16\n"
"    keyword \\\\r brightgreen/16\n"
"    keyword \\\\0 brightgreen/16\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.(man|[0-9n])$ NROFF\\sSource\n"
"\n"
"context default\n"
"    keyword \\\\fP brightgreen/6\n"
"    keyword \\\\fR brightgreen/6\n"
"    keyword \\\\fB brightgreen/6\n"
"    keyword \\\\fI brightgreen/6\n"
"    keyword linestart .AS cyan/5\n"
"    keyword linestart .Ar cyan/5\n"
"    keyword linestart .At cyan/5\n"
"    keyword linestart .BE cyan/5\n"
"    keyword linestart .BH cyan/5\n"
"    keyword linestart .BI cyan/5\n"
"    keyword linestart .BR cyan/5\n"
"    keyword linestart .BS cyan/5\n"
"    keyword linestart .Bd cyan/5\n"
"    keyword linestart .Bk cyan/5\n"
"    keyword linestart .Bl cyan/5\n"
"    keyword linestart .Bu cyan/5\n"
"    keyword linestart .Bx cyan/5\n"
"    keyword linestart .CE cyan/5\n"
"    keyword linestart .CM cyan/5\n"
"    keyword linestart .CS cyan/5\n"
"    keyword linestart .CT cyan/5\n"
"    keyword linestart .CW cyan/5\n"
"    keyword linestart .Cm cyan/5\n"
"    keyword linestart .Co cyan/5\n"
"    keyword linestart .DA cyan/5\n"
"    keyword linestart .DE cyan/5\n"
"    keyword linestart .DS cyan/5\n"
"    keyword linestart .DT cyan/5\n"
"    keyword linestart .Dd cyan/5\n"
"    keyword linestart .De cyan/5\n"
"    keyword linestart .Dl cyan/5\n"
"    keyword linestart .Dq cyan/5\n"
"    keyword linestart .Ds cyan/5\n"
"    keyword linestart .Dt cyan/5\n"
"    keyword linestart .Dv cyan/5\n"
"    keyword linestart .EE cyan/5\n"
"    keyword linestart .EN cyan/5\n"
"    keyword linestart .EQ cyan/5\n"
"    keyword linestart .EX cyan/5\n"
"    keyword linestart .Ed cyan/5\n"
"    keyword linestart .Ee cyan/5\n"
"    keyword linestart .Ek cyan/5\n"
"    keyword linestart .El cyan/5\n"
"    keyword linestart .Em cyan/5\n"
"    keyword linestart .En cyan/5\n"
"    keyword linestart .Ev cyan/5\n"
"    keyword linestart .Ex cyan/5\n"
"    keyword linestart .FI cyan/5\n"
"    keyword linestart .FL cyan/5\n"
"    keyword linestart .FN cyan/5\n"
"    keyword linestart .FT cyan/5\n"
"    keyword linestart .Fi cyan/5\n"
"    keyword linestart .Fl cyan/5\n"
"    keyword linestart .Fn cyan/5\n"
"    keyword linestart .HP cyan/5\n"
"    keyword linestart .HS cyan/5\n"
"    keyword linestart .Hh cyan/5\n"
"    keyword linestart .Hi cyan/5\n"
"    keyword linestart .IB cyan/5\n"
"    keyword linestart .IP cyan/5\n"
"    keyword linestart .IR cyan/5\n"
"    keyword linestart .IX cyan/5\n"
"    keyword linestart .Ic cyan/5\n"
"    keyword linestart .Id cyan/5\n"
"    keyword linestart .Ip cyan/5\n"
"    keyword linestart .It cyan/5\n"
"    keyword linestart .LI cyan/5\n"
"    keyword linestart .LO cyan/5\n"
"    keyword linestart .LP cyan/5\n"
"    keyword linestart .LR cyan/5\n"
"    keyword linestart .Li cyan/5\n"
"    keyword linestart .MF cyan/5\n"
"    keyword linestart .ML cyan/5\n"
"    keyword linestart .MU cyan/5\n"
"    keyword linestart .MV cyan/5\n"
"    keyword linestart .N cyan/5\n"
"    keyword linestart .NF cyan/5\n"
"    keyword linestart .Nd cyan/5\n"
"    keyword linestart .Nm cyan/5\n"
"    keyword linestart .No cyan/5\n"
"    keyword linestart .OP cyan/5\n"
"    keyword linestart .Oc cyan/5\n"
"    keyword linestart .Oo cyan/5\n"
"    keyword linestart .Op cyan/5\n"
"    keyword linestart .Os cyan/5\n"
"    keyword linestart .PD cyan/5\n"
"    keyword linestart .PN cyan/5\n"
"    keyword linestart .PP cyan/5\n"
"    keyword linestart .PU cyan/5\n"
"    keyword linestart .Pa cyan/5\n"
"    keyword linestart .Pf cyan/5\n"
"    keyword linestart .Pp cyan/5\n"
"    keyword linestart .Pq cyan/5\n"
"    keyword linestart .Pr cyan/5\n"
"    keyword linestart .Ps cyan/5\n"
"    keyword linestart .Ql cyan/5\n"
"    keyword linestart .RB cyan/5\n"
"    keyword linestart .RE cyan/5\n"
"    keyword linestart .RI cyan/5\n"
"    keyword linestart .RS cyan/5\n"
"    keyword linestart .RT cyan/5\n"
"    keyword linestart .Re cyan/5\n"
"    keyword linestart .Rs cyan/5\n"
"    keyword linestart .SB cyan/5\n"
"    keyword linestart .SH cyan/5\n"
"    keyword linestart .SM cyan/5\n"
"    keyword linestart .SP cyan/5\n"
"    keyword linestart .SS cyan/5\n"
"    keyword linestart .Sa cyan/5\n"
"    keyword linestart .Sh cyan/5\n"
"    keyword linestart .Sm cyan/5\n"
"    keyword linestart .Sp cyan/5\n"
"    keyword linestart .Sq cyan/5\n"
"    keyword linestart .Ss cyan/5\n"
"    keyword linestart .St cyan/5\n"
"    keyword linestart .Sx cyan/5\n"
"    keyword linestart .Sy cyan/5\n"
"    keyword linestart .TE cyan/5\n"
"    keyword linestart .TH cyan/5\n"
"    keyword linestart .TP cyan/5\n"
"    keyword linestart .TQ cyan/5\n"
"    keyword linestart .TS cyan/5\n"
"    keyword linestart .Tn cyan/5\n"
"    keyword linestart .Tp cyan/5\n"
"    keyword linestart .UC cyan/5\n"
"    keyword linestart .Uh cyan/5\n"
"    keyword linestart .Ux cyan/5\n"
"    keyword linestart .VE cyan/5\n"
"    keyword linestart .VS cyan/5\n"
"    keyword linestart .Va cyan/5\n"
"    keyword linestart .Vb cyan/5\n"
"    keyword linestart .Ve cyan/5\n"
"    keyword linestart .Xc cyan/5\n"
"    keyword linestart .Xe cyan/5\n"
"    keyword linestart .Xr cyan/5\n"
"    keyword linestart .YN cyan/5\n"
"    keyword linestart .ad cyan/5\n"
"    keyword linestart .am cyan/5\n"
"    keyword linestart .bd cyan/5\n"
"    keyword linestart .bp cyan/5\n"
"    keyword linestart .br cyan/5\n"
"    keyword linestart .ce cyan/5\n"
"    keyword linestart .cs cyan/5\n"
"    keyword linestart .de cyan/5\n"
"    keyword linestart .ds cyan/5\n"
"    keyword linestart .ec cyan/5\n"
"    keyword linestart .eh cyan/5\n"
"    keyword linestart .el cyan/5\n"
"    keyword linestart .eo cyan/5\n"
"    keyword linestart .ev cyan/5\n"
"    keyword linestart .fc cyan/5\n"
"    keyword linestart .fi cyan/5\n"
"    keyword linestart .ft cyan/5\n"
"    keyword linestart .hy cyan/5\n"
"    keyword linestart .iX cyan/5\n"
"    keyword linestart .ie cyan/5\n"
"    keyword linestart .if cyan/5\n"
"    keyword linestart .ig cyan/5\n"
"    keyword linestart .in cyan/5\n"
"    keyword linestart .ll cyan/5\n"
"    keyword linestart .lp cyan/5\n"
"    keyword linestart .ls cyan/5\n"
"    keyword linestart .mk cyan/5\n"
"    keyword linestart .na cyan/5\n"
"    keyword linestart .ne cyan/5\n"
"    keyword linestart .nf cyan/5\n"
"    keyword linestart .nh cyan/5\n"
"    keyword linestart .nr cyan/5\n"
"    keyword linestart .ns cyan/5\n"
"    keyword linestart .oh cyan/5\n"
"    keyword linestart .ps cyan/5\n"
"    keyword linestart .re cyan/5\n"
"    keyword linestart .rm cyan/5\n"
"    keyword linestart .rn cyan/5\n"
"    keyword linestart .rr cyan/5\n"
"    keyword linestart .so cyan/5\n"
"    keyword linestart .sp cyan/5\n"
"    keyword linestart .ss cyan/5\n"
"    keyword linestart .ta cyan/5\n"
"    keyword linestart .ti cyan/5\n"
"    keyword linestart .tm cyan/5\n"
"    keyword linestart .tr cyan/5\n"
"    keyword linestart .ul cyan/5\n"
"    keyword linestart .vs cyan/5\n"
"    keyword linestart .zZ cyan/5\n"
"    keyword linestart .e cyan/5\n"
"    keyword linestart .d cyan/5\n"
"    keyword linestart .h cyan/5\n"
"    keyword linestart .B cyan/5\n"
"    keyword linestart .I cyan/5\n"
"    keyword linestart .R cyan/5\n"
"    keyword linestart .P cyan/5\n"
"    keyword linestart .L cyan/5\n"
"    keyword linestart .V cyan/5\n"
"    keyword linestart .F cyan/5\n"
"    keyword linestart .T cyan/5\n"
"    keyword linestart .X cyan/5\n"
"    keyword linestart .Y cyan/5\n"
"    keyword linestart .b cyan/5\n"
"    keyword linestart .l cyan/5\n"
"    keyword linestart .i cyan/5\n"
"\n"
"context exclusive linestart .SH \\n brightred/18\n"
"context exclusive linestart .TH \\n brightred/18\n"
"context exclusive linestart .B \\n magenta/23\n"
"context exclusive linestart .I \\n yellow/24\n"
"context exclusive linestart .nf linestart .fi green/15\n"
"\n"
"# font changes should end in a \\fP\n"
"context exclusive \\\\fB \\\\fP magenta/23\n"
"context exclusive \\\\fI \\\\fP yellow/24\n"
"context linestart .\\\\\" \\n brown/22\n"
"\n"
"###############################################################################\n"
"# Assumes you've set a dark background, e.g. navy blue.\n"
"file ..\\*\\\\.(htm|html|HTM|HTML)$ HTML\\sFile\n"
"\n"
"context default white/25\n"
"    keyword whole &*; brightgreen/16\n"
"context <!-- --> white/26\n"
"context < > brightcyan/17\n"
"    keyword \"http:*\" magenta/22\n"
"    keyword \"ftp:*\" magenta/22\n"
"    keyword \"mailto:*\" magenta/22\n"
"    keyword \"gopher:*\" magenta/22\n"
"    keyword \"telnet:*\" magenta/22\n"
"    keyword \"file:*\" magenta/22\n"
"    keyword \"*.gif\" brightred/19\n"
"    keyword \"*.jpg\" brightred/19\n"
"    keyword \"*.png\" brightred/19\n"
"    keyword \"*\" cyan/5\n"
"\n"
"###############################################################################\n"
"# Pascal (BP7 IDE alike)\n"
"file ..\\*\\\\.(pp|PP|pas|PAS|)$ Pascal Program\n"
"context default yellow/24\n"
"    keyword whole absolute white/25\n"
"    keyword whole and white/25\n"
"    keyword whole array white/25\n"
"    keyword whole as white/25\n"
"    keyword whole asm white/25\n"
"    keyword whole assembler white/25\n"
"    keyword whole begin white/25\n"
"    keyword whole break white/25\n"
"    keyword whole case white/25\n"
"    keyword whole class white/25\n"
"    keyword whole const white/25\n"
"    keyword whole continue white/25\n"
"    keyword whole constructor white/25\n"
"    keyword whole destructor white/25\n"
"    keyword whole dispose white/25\n"
"    keyword whole div white/25\n"
"    keyword whole do white/25\n"
"    keyword whole downto white/25\n"
"    keyword whole else white/25\n"
"    keyword whole end white/25\n"
"    keyword whole except white/25\n"
"    keyword whole exit white/25\n"
"    keyword whole export white/25\n"
"    keyword whole exports white/25\n"
"    keyword whole external white/25\n"
"    keyword whole fail white/25\n"
"    keyword whole far white/25\n"
"    keyword whole false white/25\n"
"    keyword whole file white/25\n"
"    keyword whole finally white/25\n"
"    keyword whole for white/25\n"
"    keyword whole forward white/25\n"
"    keyword whole function white/25\n"
"    keyword whole goto white/25\n"
"    keyword whole if white/25\n"
"    keyword whole implementation white/25\n"
"    keyword whole in white/25\n"
"    keyword whole inherited white/25\n"
"    keyword whole initialization white/25\n"
"    keyword whole inline white/25\n"
"    keyword whole interface white/25\n"
"    keyword whole interrupt white/25\n"
"    keyword whole is white/25\n"
"    keyword whole label white/25\n"
"    keyword whole library white/25\n"
"    keyword whole mod white/25\n"
"    keyword whole near white/25\n"
"    keyword whole new white/25\n"
"    keyword whole nil white/25\n"
"    keyword whole not white/25\n"
"    keyword whole object white/25\n"
"    keyword whole of white/25\n"
"    keyword whole on white/25\n"
"    keyword whole operator white/25\n"
"    keyword whole or white/25\n"
"    keyword whole otherwise white/25\n"
"    keyword whole packed white/25\n"
"    keyword whole procedure white/25\n"
"    keyword whole program white/25\n"
"    keyword whole property white/25\n"
"    keyword whole raise white/25\n"
"    keyword whole record white/25\n"
"    keyword whole repeat white/25\n"
"    keyword whole self white/25\n"
"    keyword whole set white/25\n"
"    keyword whole shl white/25\n"
"    keyword whole shr white/25\n"
"    keyword whole string white/25\n"
"    keyword whole then white/25\n"
"    keyword whole to white/25\n"
"    keyword whole true white/25\n"
"    keyword whole try white/25\n"
"    keyword whole type white/25\n"
"    keyword whole unit white/25\n"
"    keyword whole until white/25\n"
"    keyword whole uses white/25\n"
"    keyword whole var white/25\n"
"    keyword whole virtual white/25\n"
"    keyword whole while white/25\n"
"    keyword whole with white/25\n"
"    keyword whole xor white/25\n"
"    keyword whole .. white/25\n"
"    \n"
"    keyword > cyan/5\n"
"    keyword < cyan/5\n"
"    keyword \\+ cyan/5\n"
"    keyword - cyan/5\n"
"    keyword / cyan/5\n"
"    keyword % cyan/5\n"
"    keyword = cyan/5\n"
"    keyword [ cyan/5\n"
"    keyword ] cyan/5\n"
"    keyword ( cyan/5\n"
"    keyword ) cyan/5\n"
"    keyword , cyan/5\n"
"    keyword . cyan/5\n"
"    keyword : cyan/5\n"
"    keyword ; cyan/5\n"
"    keyword {$*} brightred/19\n"
"\n"
"context ' ' brightcyan/22\n"
"context // \\n brown/22\n"
"context (\\* \\*) brown/22\n"
"# context {$ } brightred/19\n"
"#    keyword \\[ABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\[-\\+\\] brightgreen/16\n"
"#    keyword $* brightgreen/16\n"
"context { } lightgray/22\n"
"\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.tex$ LaTeX\\s2.09\\sDocument\n"
"context default\n"
"wholechars left \\\\\n"
"wholechars right abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\n"
"\n"
"# type style\n"
"    keyword whole \\\\tiny yellow/24\n"
"    keyword whole \\\\scriptsize yellow/24\n"
"    keyword whole \\\\footnotesize yellow/24\n"
"    keyword whole \\\\small yellow/24\n"
"    keyword whole \\\\normalsize yellow/24\n"
"    keyword whole \\\\large yellow/24\n"
"    keyword whole \\\\Large yellow/24\n"
"    keyword whole \\\\LARGE yellow/24\n"
"    keyword whole \\\\huge yellow/24\n"
"    keyword whole \\\\Huge yellow/24\n"
"\n"
"# accents and symbols\n"
"    keyword whole \\\\`{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\'{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\^{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\\"{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\~{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\={\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\.{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\u{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\v{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\H{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\t{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\c{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\d{\\[aeiouAEIOU\\]} yellow/24\n"
"    keyword whole \\\\b{\\[aeiouAEIOU\\]} yellow/24\n"
"\n"
"    keyword whole \\\\dag yellow/24\n"
"    keyword whole \\\\ddag yellow/24\n"
"    keyword whole \\\\S yellow/24\n"
"    keyword whole \\\\P yellow/24\n"
"    keyword whole \\\\copyright yellow/24\n"
"    keyword whole \\\\pounds yellow/24\n"
"\n"
"# sectioning and table of contents\n"
"    keyword whole \\\\part[*]{*} brightred/19\n"
"    keyword whole \\\\part{*} brightred/19\n"
"    keyword whole \\\\part\\*{*} brightred/19\n"
"    keyword whole \\\\chapter[*]{*} brightred/19\n"
"    keyword whole \\\\chapter{*} brightred/19\n"
"    keyword whole \\\\chapter\\*{*} brightred/19\n"
"    keyword whole \\\\section[*]{*} brightred/19\n"
"    keyword whole \\\\section{*} brightred/19\n"
"    keyword whole \\\\section\\*{*} brightred/19\n"
"    keyword whole \\\\subsection[*]{*} brightred/19\n"
"    keyword whole \\\\subsection{*} brightred/19\n"
"    keyword whole \\\\subsection\\*{*} brightred/19\n"
"    keyword whole \\\\subsubsection[*]{*} brightred/19\n"
"    keyword whole \\\\subsubsection{*} brightred/19\n"
"    keyword whole \\\\subsubsection\\*{*} brightred/19\n"
"    keyword whole \\\\paragraph[*]{*} brightred/19\n"
"    keyword whole \\\\paragraph{*} brightred/19\n"
"    keyword whole \\\\paragraph\\*{*} brightred/19\n"
"    keyword whole \\\\subparagraph[*]{*} brightred/19\n"
"    keyword whole \\\\subparagraph{*} brightred/19\n"
"    keyword whole \\\\subparagraph\\*{*} brightred/19\n"
"\n"
"    keyword whole \\\\appendix brightred/19\n"
"    keyword whole \\\\tableofcontents brightred/19\n"
"\n"
"# misc\n"
"    keyword whole \\\\item[*] yellow/24\n"
"    keyword whole \\\\item yellow/24\n"
"    keyword whole \\\\\\\\ yellow/24\n"
"    keyword \\\\\\s yellow/24 black/0\n"
"    keyword %% yellow/24\n"
"\n"
"# docuement and page styles    \n"
"    keyword whole \\\\documentstyle[*]{*} yellow/20\n"
"    keyword whole \\\\documentstyle{*} yellow/20\n"
"    keyword whole \\\\pagestyle{*} yellow/20\n"
"\n"
"# cross references\n"
"    keyword whole \\\\label{*} yellow/24\n"
"    keyword whole \\\\ref{*} yellow/24\n"
"\n"
"# bibliography and citations\n"
"    keyword whole \\\\bibliography{*} yellow/24\n"
"    keyword whole \\\\bibitem[*]{*} yellow/24\n"
"    keyword whole \\\\bibitem{*} yellow/24\n"
"    keyword whole \\\\cite[*]{*} yellow/24\n"
"    keyword whole \\\\cite{*} yellow/24\n"
"\n"
"# splitting the input\n"
"    keyword whole \\\\input{*} yellow/20\n"
"    keyword whole \\\\include{*} yellow/20\n"
"    keyword whole \\\\includeonly{*} yellow/20\n"
"\n"
"# line breaking\n"
"    keyword whole \\\\linebreak[\\[01234\\]] yellow/24\n"
"    keyword whole \\\\nolinebreak[\\[01234\\]] yellow/24\n"
"    keyword whole \\\\linebreak yellow/24\n"
"    keyword whole \\\\nolinebreak yellow/24\n"
"    keyword whole \\\\[+] yellow/24\n"
"    keyword whole \\\\- yellow/24\n"
"    keyword whole \\\\sloppy yellow/24\n"
"\n"
"# page breaking\n"
"    keyword whole \\\\pagebreak[\\[01234\\]] yellow/24\n"
"    keyword whole \\\\nopagebreak[\\[01234\\]] yellow/24\n"
"    keyword whole \\\\pagebreak yellow/24\n"
"    keyword whole \\\\nopagebreak yellow/24\n"
"    keyword whole \\\\samepage yellow/24\n"
"    keyword whole \\\\newpage yellow/24\n"
"    keyword whole \\\\clearpage yellow/24\n"
"\n"
"# defintiions\n"
"    keyword \\\\newcommand{*}[*] cyan/5\n"
"    keyword \\\\newcommand{*} cyan/5\n"
"    keyword \\\\newenvironment{*}[*]{*} cyan/5\n"
"    keyword \\\\newenvironment{*}{*} cyan/5\n"
"\n"
"# boxes\n"
"\n"
"# begins and ends\n"
"    keyword \\\\begin{document} brightred/14\n"
"    keyword \\\\begin{equation} brightred/14\n"
"    keyword \\\\begin{eqnarray} brightred/14\n"
"    keyword \\\\begin{quote} brightred/14\n"
"    keyword \\\\begin{quotation} brightred/14\n"
"    keyword \\\\begin{center} brightred/14\n"
"    keyword \\\\begin{verse} brightred/14\n"
"    keyword \\\\begin{verbatim} brightred/14\n"
"    keyword \\\\begin{itemize} brightred/14\n"
"    keyword \\\\begin{enumerate} brightred/14\n"
"    keyword \\\\begin{description} brightred/14\n"
"    keyword \\\\begin{array} brightred/14\n"
"    keyword \\\\begin{tabular} brightred/14\n"
"    keyword \\\\begin{thebibliography}{*} brightred/14\n"
"    keyword \\\\begin{sloppypar} brightred/14\n"
"\n"
"    keyword \\\\end{document} brightred/14\n"
"    keyword \\\\end{equation} brightred/14\n"
"    keyword \\\\end{eqnarray} brightred/14\n"
"    keyword \\\\end{quote} brightred/14\n"
"    keyword \\\\end{quotation} brightred/14\n"
"    keyword \\\\end{center} brightred/14\n"
"    keyword \\\\end{verse} brightred/14\n"
"    keyword \\\\end{verbatim} brightred/14\n"
"    keyword \\\\end{itemize} brightred/14\n"
"    keyword \\\\end{enumerate} brightred/14\n"
"    keyword \\\\end{description} brightred/14\n"
"    keyword \\\\end{array} brightred/14\n"
"    keyword \\\\end{tabular} brightred/14\n"
"    keyword \\\\end{thebibliography}{*} brightred/14\n"
"    keyword \\\\end{sloppypar} brightred/14\n"
"\n"
"    keyword \\\\begin{*} brightcyan/16\n"
"    keyword \\\\end{*} brightcyan/16\n"
"\n"
"    keyword \\\\theorem{*}{*} yellow/24\n"
"\n"
"# if all else fails\n"
"    keyword whole \\\\+[*]{*}{*}{*} brightcyan/17\n"
"    keyword whole \\\\+[*]{*}{*} brightcyan/17\n"
"    keyword whole \\\\+{*}{*}{*}{*} brightcyan/17\n"
"    keyword whole \\\\+{*}{*}{*} brightcyan/17\n"
"    keyword whole \\\\+{*}{*} brightcyan/17\n"
"    keyword whole \\\\+{*} brightcyan/17\n"
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\n brightcyan/17\n"
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\s brightcyan/17\n"
"    keyword \\\\\\[abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\\]\\t brightcyan/17\n"
"\n"
"context \\\\pagenumbering{ } yellow/20\n"
"    keyword arabic brightcyan/17\n"
"    keyword roman brightcyan/17\n"
"    keyword alph brightcyan/17\n"
"    keyword Roman brightcyan/17\n"
"    keyword Alph brightcyan/17\n"
"\n"
"context % \\n brown/22\n"
"\n"
"# mathematical formulas\n"
"context $ $ brightgreen/6\n"
"context exclusive \\\\begin{equation} \\\\end{equation} brightgreen/6\n"
"context exclusive \\\\begin{eqnarray} \\\\end{eqnarray} brightgreen/6\n"
"\n"
"\n"
"###############################################################################\n"
"file ..\\*\\\\.([chC]|CC|cxx|cc|cpp|CPP|CXX)$ C/C\\+\\+\\sProgram\n"
"context default\n"
"    keyword whole auto yellow/24\n"
"    keyword whole break yellow/24\n"
"    keyword whole case yellow/24\n"
"    keyword whole char yellow/24\n"
"    keyword whole const yellow/24\n"
"    keyword whole continue yellow/24\n"
"    keyword whole default yellow/24\n"
"    keyword whole do yellow/24\n"
"    keyword whole double yellow/24\n"
"    keyword whole else yellow/24\n"
"    keyword whole enum yellow/24\n"
"    keyword whole extern yellow/24\n"
"    keyword whole float yellow/24\n"
"    keyword whole for yellow/24\n"
"    keyword whole goto yellow/24\n"
"    keyword whole if yellow/24\n"
"    keyword whole int yellow/24\n"
"    keyword whole long yellow/24\n"
"    keyword whole register yellow/24\n"
"    keyword whole return yellow/24\n"
"    keyword whole short yellow/24\n"
"    keyword whole signed yellow/24\n"
"    keyword whole sizeof yellow/24\n"
"    keyword whole static yellow/24\n"
"    keyword whole struct yellow/24\n"
"    keyword whole switch yellow/24\n"
"    keyword whole typedef yellow/24\n"
"    keyword whole union yellow/24\n"
"    keyword whole unsigned yellow/24\n"
"    keyword whole void yellow/24\n"
"    keyword whole volatile yellow/24\n"
"    keyword whole while yellow/24\n"
"    keyword whole asm yellow/24\n"
"    keyword whole catch yellow/24\n"
"    keyword whole class yellow/24\n"
"    keyword whole friend yellow/24\n"
"    keyword whole delete yellow/24\n"
"    keyword whole inline yellow/24\n"
"    keyword whole new yellow/24\n"
"    keyword whole operator yellow/24\n"
"    keyword whole private yellow/24\n"
"    keyword whole protected yellow/24\n"
"    keyword whole public yellow/24\n"
"    keyword whole this yellow/24\n"
"    keyword whole throw yellow/24\n"
"    keyword whole template yellow/24\n"
"    keyword whole try yellow/24\n"
"    keyword whole virtual yellow/24\n"
"    keyword whole bool yellow/24\n"
"    keyword whole const_cast yellow/24\n"
"    keyword whole dynamic_cast yellow/24\n"
"    keyword whole explicit yellow/24\n"
"    keyword whole false yellow/24\n"
"    keyword whole mutable yellow/24\n"
"    keyword whole namespace yellow/24\n"
"    keyword whole reinterpret_cast yellow/24\n"
"    keyword whole static_cast yellow/24\n"
"    keyword whole true yellow/24\n"
"    keyword whole typeid yellow/24\n"
"    keyword whole typename yellow/24\n"
"    keyword whole using yellow/24\n"
"    keyword whole wchar_t yellow/24\n"
"    keyword whole ... yellow/24\n"
"\n"
"    keyword /\\* brown/22\n"
"    keyword \\*/ brown/22\n"
"\n"
"    keyword '\\s' brightgreen/16\n"
"    keyword '+' brightgreen/16\n"
"    keyword > yellow/24\n"
"    keyword < yellow/24\n"
"    keyword \\+ yellow/24\n"
"    keyword - yellow/24\n"
"    keyword \\* yellow/24\n"
"#    keyword / yellow/24\n"
"    keyword % yellow/24\n"
"    keyword = yellow/24\n"
"    keyword != yellow/24\n"
"    keyword == yellow/24\n"
"    keyword { brightcyan/14\n"
"    keyword } brightcyan/14\n"
"    keyword ( brightcyan/15\n"
"    keyword ) brightcyan/15\n"
"    keyword [ brightcyan/14\n"
"    keyword ] brightcyan/14\n"
"    keyword , brightcyan/14\n"
"    keyword : brightcyan/14\n"
"    keyword ; brightmagenta/19\n"
"context exclusive /\\* \\*/ brown/22\n"
"context // \\n brown/22\n"
"context linestart # \\n brightred/18\n"
"    keyword \\\\\\n yellow/24\n"
"    keyword /\\**\\*/ brown/22\n"
"    keyword \"+\" red/19\n"
"    keyword <+> red/19\n"
"context \" \" green/6\n"
"    keyword \\\\\" brightgreen/16\n"
"    keyword %% brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]e brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]E brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]f brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]g brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[L\\]G brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]d brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]i brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]o brightgreen/16\n"
"    keyword %\\[0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]u brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]x brightgreen/16\n"
"    keyword %\\[#0\\s-\\+,\\]\\[0123456789\\]\\[.\\]\\[0123456789\\]\\[hl\\]X brightgreen/16\n"
"    keyword %\\[hl\\]n brightgreen/16\n"
"    keyword %\\[.\\]\\[0123456789\\]s brightgreen/16\n"
"    keyword %[*] brightgreen/16\n"
"    keyword %c brightgreen/16\n"
"    keyword \\\\\\\\ brightgreen/16\n"
"    keyword \\\\' brightgreen/16\n"
"    keyword \\\\a brightgreen/16\n"
"    keyword \\\\b brightgreen/16\n"
"    keyword \\\\t brightgreen/16\n"
"    keyword \\\\n brightgreen/16\n"
"    keyword \\\\v brightgreen/16\n"
"    keyword \\\\f brightgreen/16\n"
"    keyword \\\\r brightgreen/16\n"
"    keyword \\\\0 brightgreen/16\n"
"\n"
"###############################################################################\n"
"file .\\*ChangeLog$ GNU\\sDistribution\\sChangeLog\\sFile\n"
"\n"
"context default\n"
"    keyword \\s+() brightmagenta/23\n"
"    keyword \\t+() brightmagenta/23\n"
"\n"
"context linestart \\t\\* : brightcyan/17\n"
"context linestart \\s\\s\\s\\s\\s\\s\\s\\s\\* : brightcyan/17\n"
"\n"
"context linestart 19+-+\\s \\n            yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart 20+-+\\s \\n            yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Mon\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Tue\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Wed\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Thu\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Fri\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Sat\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"context linestart Sun\\s+\\s+\\s+\\s \\n     yellow/24\n"
"    keyword <+@+> 			brightred/19\n"
"\n"
"\n"
"###############################################################################\n"
"file .\\*Makefile[\\\\\\.a-z]\\*$ Makefile\n"
"\n"
"context default\n"
"    keyword $(*) yellow/24\n"
"    keyword ${*} brightgreen/16\n"
"    keyword whole linestart include magenta\n"
"    keyword whole linestart endif magenta\n"
"    keyword whole linestart ifeq magenta\n"
"    keyword whole linestart ifneq magenta\n"
"    keyword whole linestart else magenta\n"
"    keyword linestart \\t lightgray/13 red\n"
"    keyword whole .PHONY white/25\n"
"    keyword whole .NOEXPORT white/25\n"
"    keyword = white/25\n"
"    keyword : yellow/24\n"
"    keyword \\\\\\n yellow/24\n"
"# this handles strange cases like @something@@somethingelse@ properly\n"
"    keyword whole @+@ brightmagenta/23 black/0\n"
"    keyword @+@ brightmagenta/23 black/0\n"
"\n"
"context linestart # \\n brown/22\n"
"    keyword whole @+@ brightmagenta/23 black/0\n"
"    keyword @+@ brightmagenta/23 black/0\n"
"\n"
"context exclusive = \\n brightcyan/17\n"
"    keyword \\\\\\n yellow/24\n"
"    keyword $(*) yellow/24\n"
"    keyword ${*} brightgreen/16\n"
"    keyword linestart \\t lightgray/13 red\n"
"    keyword whole @+@ brightmagenta/23 black/0\n"
"    keyword @+@ brightmagenta/23 black/0\n"
"\n"
"context exclusive linestart \\t \\n\n"
"    keyword \\\\\\n yellow/24\n"
"    keyword $(*) yellow/24\n"
"    keyword ${*} brightgreen/16\n"
"    keyword linestart \\t lightgray/13 red\n"
"    keyword whole @+@ brightmagenta/23 black/0\n"
"    keyword @+@ brightmagenta/23 black/0\n"
"\n"
"###############################################################################\n"
"\n"
"file .\\*syntax$ Syntax\\sHighlighting\\sdefinitions\n"
"\n"
"context default\n"
"    keyword whole keyw\\ord yellow/24\n"
"    keyword whole whole\\[\\t\\s\\]l\\inestart brightcyan/17\n"
"    keyword whole whole\\[\\t\\s\\]l\\inestart brightcyan/17\n"
"    keyword whole wh\\oleleft\\[\\t\\s\\]l\\inestart brightcyan/17\n"
"    keyword whole wh\\oleright\\[\\t\\s\\]l\\inestart brightcyan/17\n"
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\ole\n"
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\ole\n"
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\oleleft\n"
"    keyword whole l\\inestart\\[\\t\\s\\]wh\\oleright\n"
"    keyword wholeleft whole\\s brightcyan/17\n"
"    keyword wholeleft whole\\t brightcyan/17\n"
"    keyword whole wh\\oleleft brightcyan/17\n"
"    keyword whole wh\\oleright brightcyan/17\n"
"    keyword whole lin\\[e\\]start brightcyan/17\n"
"    keyword whole c\\ontext\\[\\t\\s\\]exclusive brightred/18\n"
"    keyword whole c\\ontext\\[\\t\\s\\]default brightred/18\n"
"    keyword whole c\\ontext brightred/18\n"
"    keyword whole wh\\olechars\\[\\t\\s\\]left brightcyan/17\n"
"    keyword whole wh\\olechars\\[\\t\\s\\]right brightcyan/17\n"
"    keyword whole wh\\olechars brightcyan/17\n"
"    keyword whole f\\ile brightgreen/6\n"
"\n"
"    keyword whole 0 lightgray/0	blue/26\n"
"    keyword whole 1 lightgray/1	blue/26\n"
"    keyword whole 2 lightgray/2	blue/26\n"
"    keyword whole 3 lightgray/3	blue/26\n"
"    keyword whole 4 lightgray/4	blue/26\n"
"    keyword whole 5 lightgray/5	blue/26\n"
"    keyword whole 6 lightgray/6\n"
"    keyword whole 7 lightgray/7\n"
"    keyword whole 8 lightgray/8\n"
"    keyword whole 9 lightgray/9\n"
"    keyword whole 10 lightgray/10\n"
"    keyword whole 11 lightgray/11\n"
"    keyword whole 12 lightgray/12\n"
"    keyword whole 13 lightgray/13\n"
"    keyword whole 14 lightgray/14\n"
"    keyword whole 15 lightgray/15\n"
"    keyword whole 16 lightgray/16\n"
"    keyword whole 17 lightgray/17\n"
"    keyword whole 18 lightgray/18\n"
"    keyword whole 19 lightgray/19\n"
"    keyword whole 20 lightgray/20\n"
"    keyword whole 21 lightgray/21\n"
"    keyword whole 22 lightgray/22\n"
"    keyword whole 23 lightgray/23\n"
"    keyword whole 24 lightgray/24\n"
"    keyword whole 25 lightgray/25\n"
"    keyword whole 26 lightgray/26\n"
"\n"
"    keyword wholeleft black\\/ black/0\n"
"    keyword wholeleft red\\/ red/DarkRed\n"
"    keyword wholeleft green\\/ green/green3\n"
"    keyword wholeleft brown\\/ brown/saddlebrown\n"
"    keyword wholeleft blue\\/ blue/blue3\n"
"    keyword wholeleft magenta\\/ magenta/magenta3\n"
"    keyword wholeleft cyan\\/ cyan/cyan3\n"
"    keyword wholeleft lightgray\\/ lightgray/lightgray\n"
"    keyword wholeleft gray\\/ gray/gray\n"
"    keyword wholeleft brightred\\/ brightred/red\n"
"    keyword wholeleft brightgreen\\/ brightgreen/green1\n"
"    keyword wholeleft yellow\\/ yellow/yellow\n"
"    keyword wholeleft brightblue\\/ brightblue/blue1\n"
"    keyword wholeleft brightmagenta\\/ brightmagenta/magenta\n"
"    keyword wholeleft brightcyan\\/ brightcyan/cyan1\n"
"    keyword wholeleft white\\/ white/26\n"
"\n"
"context linestart # \\n brown/22\n"
"\n"
"file \\.\\* Help\\ssupport\\sother\\sfile\\stypes\n"
"context default\n"
"file \\.\\* by\\scoding\\srules\\sin\\s~/.cedit/syntax.\n"
"context default\n"
"file \\.\\* See\\sman/syntax\\sin\\sthe\\ssource\\sdistribution\n"
"context default\n"
"file \\.\\* and\\sconsult\\sthe\\sman\\spage.\n"
"context default\n"
"\n";


FILE *upgrade_syntax_file (char *syntax_file)
{
    FILE *f;
    char line[80];
    f = fopen (syntax_file, "r");
    if (!f) {
	f = fopen (syntax_file, "w");
	if (!f)
	    return 0;
	fprintf (f, "%s", syntax_text);
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
    if (!SLtt_Use_Ansi_Colors)
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

