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
#include "edit.h"
#include "edit-widget.h"
#include "src/color.h"		/* use_colors */
#include "src/main.h"		/* mc_home */
#include "src/wtools.h"		/* message() */

/* bytes */
#define SYNTAX_MARKER_DENSITY 512

/*
   Mispelled words are flushed from the syntax highlighting rules
   when they have been around longer than
   TRANSIENT_WORD_TIME_OUT seconds. At a cursor rate of 30
   chars per second and say 3 chars + a space per word, we can
   accumulate 450 words absolute max with a value of 60. This is
   below this limit of 1024 words in a context.
 */
#define TRANSIENT_WORD_TIME_OUT 60

#define UNKNOWN_FORMAT "unknown"

#define MAX_WORDS_PER_CONTEXT	1024
#define MAX_CONTEXTS		128

#define RULE_ON_LEFT_BORDER 1
#define RULE_ON_RIGHT_BORDER 2

struct key_word {
    char *keyword;
    unsigned char first;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    int line_start;
    int color;
};

struct context_rule {
    char *left;
    unsigned char first_left;
    char *right;
    unsigned char first_right;
    char line_start_left;
    char line_start_right;
    int between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    int spelling;
    /* first word is word[1] */
    struct key_word **keyword;
};

struct _syntax_marker {
    long offset;
    struct syntax_rule rule;
    struct _syntax_marker *next;
};

int option_syntax_highlighting = 1;

#define syntax_g_free(x) do {if(x) {g_free(x); (x)=0;}} while (0)

static long compare_word_to_right (WEdit * edit, long i, char *text, char *whole_left, char *whole_right, int line_start)
{
    unsigned char *p, *q;
    int c, d, j;
    if (!*text)
	return -1;
    c = edit_get_byte (edit, i - 1);
    if (line_start)
	if (c != '\n')
	    return -1;
    if (whole_left)
	if (strchr (whole_left, c))
	    return -1;
    for (p = (unsigned char *) text, q = p + strlen ((char *) p); p < q; p++, i++) {
	switch (*p) {
	case '\001':
	    p++;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (!*p)
		    if (whole_right)
			if (!strchr (whole_right, c))
			    break;
		if (c == *p)
		    break;
		if (c == '\n')
		    return -1;
		i++;
	    }
	    break;
	case '\002':
	    p++;
	    j = 0;
	    for (;;) {
		c = edit_get_byte (edit, i);
		if (c == *p) {
		    j = i;
		    if (*p == *text && !p[1])	/* handle eg '+' and @+@ keywords properly */
			break;
		}
		if (j && strchr ((char *) p + 1, c))	/* c exists further down, so it will get matched later */
		    break;
		if (c == '\n' || c == '\t' || c == ' ') {
		    if (!*p) {
			i--;
			break;
		    }
		    if (!j)
			return -1;
		    i = j;
		    break;
		}
		if (whole_right)
		    if (!strchr (whole_right, c)) {
			if (!*p) {
			    i--;
			    break;
			}
			if (!j)
			    return -1;
			i = j;
			break;
		    }
		i++;
	    }
	    break;
	case '\003':
	    p++;
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
	case '\004':
	    p++;
	    c = edit_get_byte (edit, i);
	    for (; *p != '\004'; p++)
		if (c == *p)
		    goto found_char3;
	    return -1;
	  found_char3:
	    for (; *p != '\004'; p++);
	    break;
	default:
	    if (*p != edit_get_byte (edit, i))
		return -1;
	}
    }
    if (whole_right)
	if (strchr (whole_right, edit_get_byte (edit, i)))
	    return -1;
    return i;
}

#define XXX							\
	    if (*s < '\005' || *s == (unsigned char) c)		\
		goto done;					\
	    s++;

static inline char *xx_strchr (const unsigned char *s, int c)
{
  repeat:
    XXX XXX XXX XXX XXX XXX XXX XXX;
    XXX XXX XXX XXX XXX XXX XXX XXX;
    goto repeat;
  done:
    return (char *) s;
}

static inline struct syntax_rule apply_rules_going_right (WEdit * edit, long i, struct syntax_rule rule)
{
    struct context_rule *r;
    int contextchanged = 0, c;
    int found_right = 0, found_left = 0, keyword_foundleft = 0, keyword_foundright = 0;
    int is_end;
    long end = 0;
    struct syntax_rule _rule = rule;
    if (!(c = edit_get_byte (edit, i)))
	return rule;
    is_end = (rule.end == (unsigned char) i);
/* check to turn off a keyword */
    if (_rule.keyword) {
	if (edit_get_byte (edit, i - 1) == '\n')
	    _rule.keyword = 0;
	if (is_end) {
	    _rule.keyword = 0;
	    keyword_foundleft = 1;
	}
    }
/* check to turn off a context */
    if (_rule.context && !_rule.keyword) {
	long e;
	r = edit->rules[_rule.context];
	if (r->first_right == c && !(rule.border & RULE_ON_RIGHT_BORDER) && (e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) > 0) {
	    _rule.end = e;
	    found_right = 1;
	    _rule.border = RULE_ON_RIGHT_BORDER;
	    if (r->between_delimiters)
		_rule.context = 0;
	} else if (is_end && rule.border & RULE_ON_RIGHT_BORDER) {
/* always turn off a context at 4 */
	    found_left = 1;
	    _rule.border = 0;
	    if (!keyword_foundleft)
		_rule.context = 0;
	} else if (is_end && rule.border & RULE_ON_LEFT_BORDER) {
/* never turn off a context at 2 */
	    found_left = 1;
	    _rule.border = 0;
	}
    }
/* check to turn on a keyword */
    if (!_rule.keyword) {
	char *p;
	p = (r = edit->rules[_rule.context])->keyword_first_chars;
	if (p)
	while (*(p = xx_strchr ((unsigned char *) p + 1, c))) {
	    struct key_word *k;
	    int count;
	    long e;
	    count = p - r->keyword_first_chars;
	    k = r->keyword[count];
	    e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start);
	    if (e > 0) {
		end = e;
		_rule.end = e;
		_rule.keyword = count;
		keyword_foundright = 1;
		break;
	    }
	}
    }
/* check to turn on a context */
    if (!_rule.context) {
	if (!found_left && is_end) {
	    if (rule.border & RULE_ON_RIGHT_BORDER) {
		_rule.border = 0;
		_rule.context = 0;
		contextchanged = 1;
		_rule.keyword = 0;
	    } else if (rule.border & RULE_ON_LEFT_BORDER) {
		r = edit->rules[_rule._context];
		_rule.border = 0;
		if (r->between_delimiters) {
		    long e;
		    _rule.context = _rule._context;
		    contextchanged = 1;
		    _rule.keyword = 0;
		    if (r->first_right == c && (e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_right)) >= end) {
			_rule.end = e;
			found_right = 1;
			_rule.border = RULE_ON_RIGHT_BORDER;
			_rule.context = 0;
		    }
		}
	    }
	}
	if (!found_right) {
	    int count;
	    struct context_rule **rules = edit->rules;
	    for (count = 1; rules[count]; count++) {
		r = rules[count];
		if (r->first_left == c) {
		    long e;
		    e = compare_word_to_right (edit, i, r->left, r->whole_word_chars_left, r->whole_word_chars_right, r->line_start_left);
		    if (e >= end && (!_rule.keyword || keyword_foundright)) {
			_rule.end = e;
			found_right = 1;
			_rule.border = RULE_ON_LEFT_BORDER;
			_rule._context = count;
			if (!r->between_delimiters)
			    if (!_rule.keyword) {
				_rule.context = count;
				contextchanged = 1;
			    }
			break;
		    }
		}
	    }
	}
    }
/* check again to turn on a keyword if the context switched */
    if (contextchanged && !_rule.keyword) {
	char *p;
	p = (r = edit->rules[_rule.context])->keyword_first_chars;
	while (*(p = xx_strchr ((unsigned char *) p + 1, c))) {
	    struct key_word *k;
	    int count;
	    long e;
	    count = p - r->keyword_first_chars;
	    k = r->keyword[count];
	    e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left, k->whole_word_chars_right, k->line_start);
	    if (e > 0) {
		_rule.end = e;
		_rule.keyword = count;
		break;
	    }
	}
    }
    return _rule;
}

static struct syntax_rule edit_get_rule (WEdit * edit, long byte_index)
{
    long i;
    if (byte_index > edit->last_get_rule) {
	for (i = edit->last_get_rule + 1; i <= byte_index; i++) {
	    edit->rule = apply_rules_going_right (edit, i, edit->rule);
	    if (i > (edit->syntax_marker ? edit->syntax_marker->offset + SYNTAX_MARKER_DENSITY : SYNTAX_MARKER_DENSITY)) {
		struct _syntax_marker *s;
		s = edit->syntax_marker;
		edit->syntax_marker = g_malloc0 (sizeof (struct _syntax_marker));
		edit->syntax_marker->next = s;
		edit->syntax_marker->offset = i;
		edit->syntax_marker->rule = edit->rule;
	    }
	}
    } else if (byte_index < edit->last_get_rule) {
	struct _syntax_marker *s;
	for (;;) {
	    if (!edit->syntax_marker) {
		memset (&edit->rule, 0, sizeof (edit->rule));
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
	    syntax_g_free (edit->syntax_marker);
	    edit->syntax_marker = s;
	}
    }
    edit->last_get_rule = byte_index;
    return edit->rule;
}

static void translate_rule_to_color (WEdit * edit, struct syntax_rule rule, int *color)
{
    struct key_word *k;
    k = edit->rules[rule.context]->keyword[rule.keyword];
    *color = k->color;
}

void edit_get_syntax_color (WEdit * edit, long byte_index, int *color)
{
    if (edit->rules && byte_index < edit->last_byte && 
                         option_syntax_highlighting && use_colors) {
	translate_rule_to_color (edit, edit_get_rule (edit, byte_index), color);
    } else {
	*color = use_colors ? EDITOR_NORMAL_COLOR_INDEX : 0;
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
    p = g_malloc0 (len);

    for (;;) {
	c = fgetc (f);
	if (c == EOF) {
	    if (ferror (f)) {
		if (errno == EINTR)
		    continue;
		r = 0;
	    }
	    break;
	} else if (c == '\n') {
	    r = i + 1;		/* extra 1 for the newline just read */
	    break;
	} else {
	    if (i >= len - 1) {
		char *q;
		q = g_malloc0 (len * 2);
		memcpy (q, p, len);
		syntax_g_free (p);
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

static char *convert (char *s)
{
    char *r, *p;
    p = r = s;
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
		*p = '\003';
		break;
	    case '{':
	    case '}':
		*p = '\004';
		break;
	    case 0:
		*p = *s;
		return r;
	    default:
		*p = *s;
		break;
	    }
	    break;
	case '*':
	    *p = '\001';
	    break;
	case '+':
	    *p = '\002';
	    break;
	default:
	    *p = *s;
	    break;
	}
	s++;
	p++;
    }
    *p = '\0';
    return r;
}

#define whiteness(x) ((x) == '\t' || (x) == '\n' || (x) == ' ')

static void get_args (char *l, char **args, int *argc)
{
    *argc = 0;
    for (;;) {
	char *p = l;
	while (*p && whiteness (*p))
	    p++;
	if (!*p)
	    break;
	for (l = p + 1; *l && !whiteness (*l); l++);
	if (*l)
	    *l++ = '\0';
	*args = convert (p);
	(*argc)++;
	args++;
    }
    *args = 0;
}

#define free_args(x)
#define break_a	{result=line;break;}
#define check_a {if(!*a){result=line;break;}}
#define check_not_a {if(*a){result=line;break;}}

static int
this_try_alloc_color_pair (char *fg, char *bg)
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

static char *error_file_name = 0;

static FILE *open_include_file (char *filename)
{
    FILE *f;

    syntax_g_free (error_file_name);
    error_file_name = g_strdup (filename);
    if (*filename == PATH_SEP)
	return fopen (filename, "r");

    g_free (error_file_name);
    error_file_name = g_strconcat (home_dir, EDIT_DIR PATH_SEP_STR,
				   filename, NULL);
    f = fopen (error_file_name, "r");
    if (f)
	return f;

    g_free (error_file_name);
    error_file_name = g_strconcat (mc_home, PATH_SEP_STR "syntax" PATH_SEP_STR,
				   filename, NULL);
    return fopen (error_file_name, "r");
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
    struct context_rule **r, *c = 0;
    int num_words = -1, num_contexts = -1;
    int argc, result = 0;
    int i, j;

    args[0] = 0;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    r = edit->rules = g_malloc0 (MAX_CONTEXTS * sizeof (struct context_rule *));

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
		syntax_g_free (error_file_name);
		if (l)
		    syntax_g_free (l);
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
		syntax_g_free (error_file_name);
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
		    break_a;
		}
		a++;
		c = r[0] = g_malloc0 (sizeof (struct context_rule));
		c->left = g_strdup (" ");
		c->right = g_strdup (" ");
		num_contexts = 0;
	    } else {
		c = r[num_contexts] = g_malloc0 (sizeof (struct context_rule));
		if (!strcmp (*a, "exclusive")) {
		    a++;
		    c->between_delimiters = 1;
		}
		check_a;
		if (!strcmp (*a, "whole")) {
		    a++;
		    c->whole_word_chars_left = g_strdup (whole_left);
		    c->whole_word_chars_right = g_strdup (whole_right);
		} else if (!strcmp (*a, "wholeleft")) {
		    a++;
		    c->whole_word_chars_left = g_strdup (whole_left);
		} else if (!strcmp (*a, "wholeright")) {
		    a++;
		    c->whole_word_chars_right = g_strdup (whole_right);
		}
		check_a;
		if (!strcmp (*a, "linestart")) {
		    a++;
		    c->line_start_left = 1;
		}
		check_a;
		c->left = g_strdup (*a++);
		check_a;
		if (!strcmp (*a, "linestart")) {
		    a++;
		    c->line_start_right = 1;
		}
		check_a;
		c->right = g_strdup (*a++);
		c->first_left = *c->left;
		c->first_right = *c->right;
	    }
	    c->keyword = g_malloc0 (MAX_WORDS_PER_CONTEXT * sizeof (struct key_word *));
#if 0
	    c->max_words = MAX_WORDS_PER_CONTEXT;
#endif
	    num_words = 1;
	    c->keyword[0] = g_malloc0 (sizeof (struct key_word));
	    fg = *a;
	    if (*a)
		a++;
	    bg = *a;
	    if (*a)
		a++;
	    strcpy (last_fg, fg ? fg : "");
	    strcpy (last_bg, bg ? bg : "");
	    c->keyword[0]->color = this_try_alloc_color_pair (fg, bg);
	    c->keyword[0]->keyword = g_strdup (" ");
	    check_not_a;
	    num_contexts++;
	} else if (!strcmp (args[0], "spellcheck")) {
	    if (!c) {
		result = line;
		break;
	    }
	    c->spelling = 1;
	} else if (!strcmp (args[0], "keyword")) {
	    struct key_word *k;
	    if (num_words == -1)
		break_a;
	    check_a;
	    k = r[num_contexts - 1]->keyword[num_words] = g_malloc0 (sizeof (struct key_word));
	    if (!strcmp (*a, "whole")) {
		a++;
		k->whole_word_chars_left = g_strdup (whole_left);
		k->whole_word_chars_right = g_strdup (whole_right);
	    } else if (!strcmp (*a, "wholeleft")) {
		a++;
		k->whole_word_chars_left = g_strdup (whole_left);
	    } else if (!strcmp (*a, "wholeright")) {
		a++;
		k->whole_word_chars_right = g_strdup (whole_right);
	    }
	    check_a;
	    if (!strcmp (*a, "linestart")) {
		a++;
		k->line_start = 1;
	    }
	    check_a;
	    if (!strcmp (*a, "whole")) {
		break_a;
	    }
	    k->keyword = g_strdup (*a++);
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
	    k->color = this_try_alloc_color_pair (fg, bg);
	    check_not_a;
	    num_words++;
	} else if (*(args[0]) == '#') {
	    /* do nothing for comment */
	} else if (!strcmp (args[0], "file")) {
	    break;
	} else {		/* anything else is an error */
	    break_a;
	}
	free_args (args);
	syntax_g_free (l);
    }
    free_args (args);
    syntax_g_free (l);

    if (!edit->rules[0])
	syntax_g_free (edit->rules);

    if (result)
	return result;

    if (num_contexts == -1) {
	result = line;
	return result;
    }

    {
	char first_chars[MAX_WORDS_PER_CONTEXT + 2], *p;
	for (i = 0; edit->rules[i]; i++) {
	    c = edit->rules[i];
	    p = first_chars;
	    *p++ = (char) 1;
	    for (j = 1; c->keyword[j]; j++)
		*p++ = c->keyword[j]->first;
	    *p = '\0';
	    c->keyword_first_chars = g_malloc0 (strlen (first_chars) + 2);
	    strcpy (c->keyword_first_chars, first_chars);
	}
    }

    return result;
}

void edit_free_syntax_rules (WEdit * edit)
{
    int i, j;
    if (!edit)
	return;
    if (!edit->rules)
	return;
    edit_get_rule (edit, -1);
    syntax_g_free (edit->syntax_type);
    edit->syntax_type = 0;
    for (i = 0; edit->rules[i]; i++) {
	if (edit->rules[i]->keyword) {
	    for (j = 0; edit->rules[i]->keyword[j]; j++) {
		syntax_g_free (edit->rules[i]->keyword[j]->keyword);
		syntax_g_free (edit->rules[i]->keyword[j]->whole_word_chars_left);
		syntax_g_free (edit->rules[i]->keyword[j]->whole_word_chars_right);
		syntax_g_free (edit->rules[i]->keyword[j]);
	    }
	}
	syntax_g_free (edit->rules[i]->left);
	syntax_g_free (edit->rules[i]->right);
	syntax_g_free (edit->rules[i]->whole_word_chars_left);
	syntax_g_free (edit->rules[i]->whole_word_chars_right);
	syntax_g_free (edit->rules[i]->keyword);
	syntax_g_free (edit->rules[i]->keyword_first_chars);
	syntax_g_free (edit->rules[i]);
    }
    while (edit->syntax_marker) {
	struct _syntax_marker *s = edit->syntax_marker->next;
	syntax_g_free (edit->syntax_marker);
	edit->syntax_marker = s;
    }
    syntax_g_free (edit->rules);
}

/* returns -1 on file error, line number on error in file syntax */
static int edit_read_syntax_file (WEdit * edit, char **names, char *syntax_file, char *editor_file, char *first_line, char *type)
{
    FILE *f;
    regex_t r;
    regmatch_t pmatch[1];
    char *args[1024], *l = 0;
    int line = 0;
    int argc;
    int result = 0;
    int count = 0;
    char *lib_file;

    f = fopen (syntax_file, "r");
    if (!f){
	lib_file = concat_dir_and_file (mc_home, "syntax" PATH_SEP_STR "Syntax");
	f = fopen (lib_file, "r");
	g_free (lib_file);
	if (!f)
	    return -1;
    }
    args[0] = 0;
    for (;;) {
	line++;
	syntax_g_free (l);
	if (!read_one_line (&l, f))
	    break;
	get_args (l, args, &argc);
	if (!args[0])
	    continue;
/* looking for `file ...' lines only */
	if (strcmp (args[0], "file")) {
	    free_args (args);
	    continue;
	}
/* must have two args or report error */
	if (!args[1] || !args[2]) {
	    result = line;
	    break;
	}
	if (names) {
/* 1: just collecting a list of names of rule sets */
	    names[count++] = g_strdup (args[2]);
	    names[count] = 0;
	} else if (type) {
/* 2: rule set was explicitly specified by the caller */
	    if (!strcmp (type, args[2]))
		goto found_type;
	} else if (editor_file && edit) {
/* 3: auto-detect rule set from regular expressions */
	    int q;
	    if (regcomp (&r, args[1], REG_EXTENDED)) {
		result = line;
		break;
	    }
/* does filename match arg 1 ? */
	    q = !regexec (&r, editor_file, 1, pmatch, 0);
	    regfree (&r);
	    if (!q && args[3]) {
		if (regcomp (&r, args[3], REG_EXTENDED)) {
		    result = line;
		    break;
		}
/* does first line match arg 3 ? */
		q = !regexec (&r, first_line, 1, pmatch, 0);
		regfree (&r);
	    }
	    if (q) {
		int line_error;
	      found_type:
		line_error = edit_read_syntax_rules (edit, f);
		if (line_error) {
		    if (!error_file_name)	/* an included file */
			result = line + line_error;
		    else
			result = line_error;
		} else {
		    syntax_g_free (edit->syntax_type);
		    edit->syntax_type = g_strdup (args[2]);
/* if there are no rules then turn off syntax highlighting for speed */
		    if (!edit->rules[1])
			if (!edit->rules[0]->keyword[1] && !edit->rules[0]->spelling) {
			    edit_free_syntax_rules (edit);
			    break;
			}
		}
		break;
	    }
	}
	free_args (args);
    }
    free_args (args);
    syntax_g_free (l);
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

/*
 * Load rules into edit struct.  Either edit or names must be NULL.  If
 * edit is NULL, a list of types will be stored into names.  If type is
 * NULL, then the type will be selected according to the filename.
 */
void
edit_load_syntax (WEdit *edit, char **names, char *type)
{
    int r;
    char *f;

    edit_free_syntax_rules (edit);

    if (!use_colors)
	return;

    if (!option_syntax_highlighting)
	return;

    if (edit) {
	if (!edit->filename)
	    return;
	if (!*edit->filename && !type)
	    return;
    }
    f = catstrs (home_dir, SYNTAX_FILE, 0);
    r = edit_read_syntax_file (edit, names, f, edit ? edit->filename : 0,
			       get_first_editor_line (edit), type);
    if (r == -1) {
	edit_free_syntax_rules (edit);
	message (D_ERROR, _(" Load syntax file "),
		 _(" Cannot open file %s \n %s "), f,
		 unix_error_string (errno));
	return;
    }
    if (r) {
	edit_free_syntax_rules (edit);
	message (D_ERROR, _(" Load syntax file "),
		 _(" Error in file %s on line %d "),
		 error_file_name ? error_file_name : f, r);
	syntax_g_free (error_file_name);
	return;
    }
}
