/*
   Basic support for extended character sets.

   Written by:
   Roland Illig <roland.illig@gmx.de>, 2005.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software; you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.
 */

/** \file  ecs.c
 *  \brief Source: basic support for extended character sets
 */

#include <config.h>

#include <assert.h>
#include <ctype.h>

#include "global.h"
#include "ecs.h"

/*
 * String type conversion
 */

extern gboolean ecs_mbstr_to_str(ecs_char **ret_str, const char *s)
{
#ifdef EXTCHARSET_ENABLED
	size_t maxlen, len;
	ecs_char *str;

	maxlen = strlen(s);

	str = g_new(ecs_char, maxlen + 1);
	len = mbstowcs(str, s, maxlen + 1);
	if (len == (size_t) -1) {
		g_free(str);
		return FALSE;
	}

	assert(len <= maxlen);
	*ret_str = g_renew(ecs_char, str, len + 1);
	return TRUE;
#else
	*ret_str = g_strdup(s);
	return TRUE;
#endif
}

extern gboolean ecs_str_to_mbstr(char **ret_str, const ecs_char *s)
{
#ifdef EXTCHARSET_ENABLED
	size_t maxlen, len;
	char *str;

	maxlen = ecs_strlen(s) * MB_CUR_MAX;

	str = g_new(char, maxlen + 1);
	len = wcstombs(str, s, maxlen + 1);
	if (len == (size_t) -1) {
		g_free(str);
		return FALSE;
	}

	assert(len <= maxlen);
	*ret_str = g_renew(char, str, len + 1);
	return TRUE;
#else
	*ret_str = g_strdup(s);
	return TRUE;
#endif
}

/*
 * Character classification
 */

#ifdef EXTCHARSET_ENABLED
# ifdef HAVE_WCTYPE_H
#  include <wctype.h>
#  define ECS_CTYPE(wf, cf, c) \
	(wf(c))
# else
#  define ECS_CTYPE(wf, cf, c) \
	(((unsigned char) c != c) ? FALSE : (cf(c)))
# endif
#else
# define ECS_CTYPE(wf, cf, c) \
	(cf(c))
#endif

extern gboolean ecs_isalnum(ecs_char c)
{
	return ECS_CTYPE(iswalnum, isalnum, c);
}

extern gboolean ecs_isalpha(ecs_char c)
{
	return ECS_CTYPE(iswalpha, isalpha, c);
}

extern gboolean ecs_iscntrl(ecs_char c)
{
	return ECS_CTYPE(iswcntrl, iscntrl, c);
}

extern gboolean ecs_isdigit(ecs_char c)
{
	return ECS_CTYPE(iswdigit, isdigit, c);
}

extern gboolean ecs_isgraph(ecs_char c)
{
	return ECS_CTYPE(iswgraph, isgraph, c);
}

extern gboolean ecs_islower(ecs_char c)
{
	return ECS_CTYPE(iswlower, islower, c);
}

extern gboolean ecs_isprint(ecs_char c)
{
	return ECS_CTYPE(iswprint, isprint, c);
}

extern gboolean ecs_ispunct(ecs_char c)
{
	return ECS_CTYPE(iswpunct, ispunct, c);
}

extern gboolean ecs_isspace(ecs_char c)
{
	return ECS_CTYPE(iswspace, isspace, c);
}

extern gboolean ecs_isupper(ecs_char c)
{
	return ECS_CTYPE(iswupper, isupper, c);
}

extern gboolean ecs_isxdigit(ecs_char c)
{
	return ECS_CTYPE(iswxdigit, isxdigit, c);
}

#undef ECS_CTYPE

/*
 * ISO C90 <string.h> functions
 */

/* left out: ecs_strcpy */
/* left out: ecs_strncpy */
/* left out: ecs_strcat */
/* left out: ecs_strncat */

int
ecs_strcmp(const ecs_char *a, const ecs_char *b)
{
	size_t i;
	unsigned long ca, cb;

	for (i = 0; a[i] == b[i]; i++) {
		if (a[i] == ECS_CHAR('\0'))
			return 0;
	}
	ca = (unsigned long) a[i];
	cb = (unsigned long) b[i];
	return (ca < cb) ? -1 : (ca > cb) ? 1 : 0;
}

/* left out: ecs_strcoll */
/* left out: ecs_strncmp */
/* left out: ecs_strxfrm */

ecs_char *
ecs_strchr(const ecs_char *s, ecs_char c)
{
	size_t i;

	for (i = 0; s[i] != c; i++) {
		if (s[i] == ECS_CHAR('\0'))
			return NULL;
	}
	return (ecs_char *) s + i;
}

size_t
ecs_strcspn(const ecs_char *haystack, const ecs_char *needles)
{
	size_t i, j;

	for (i = 0; haystack[i] != ECS_CHAR('\0'); i++) {
		for (j = 0; needles[j] != ECS_CHAR('\0'); j++) {
			if (haystack[i] == needles[j])
				return i;
		}
	}
	return i;
}

/* left out: ecs_strpbrk */

ecs_char *
ecs_strrchr(const ecs_char *s, ecs_char c)
{
	ecs_char *pos;
	size_t i;

	for (i = 0, pos = NULL;; i++) {
		if (s[i] == c)
			pos = (ecs_char *) s + i;
		if (s[i] == ECS_CHAR('\0'))
			return pos;
	}
}

size_t
ecs_strspn(const ecs_char *s, const ecs_char *chars)
{
	size_t i;

	for (i = 0; s[i] != ECS_CHAR('\0'); i++) {
		if (ecs_strchr(chars, s[i]) == NULL)
			break;
	}
	return i;
}

ecs_char *
ecs_strstr(const ecs_char *s, const ecs_char *sub)
{
	size_t i, j;

	for (i = 0; s[i] != ECS_CHAR('\0'); i++) {
		for (j = 0; sub[j] != ECS_CHAR('\0'); j++) {
			if (s[i + j] != sub[j])
				goto next_i;
		}
		return (ecs_char *) s + i;
	next_i:
		continue;
	}
	return NULL;
}

/* left out: ecs_strtok */

size_t
ecs_strlen(const ecs_char *s)
{
	size_t i;

	for (i = 0; s[i] != ECS_CHAR('\0'); i++)
		continue;
	return i;
}

/*
 * Other functions
 */

ecs_char *ecs_xstrdup(const ecs_char *s)
{
	ecs_char *retval;
	size_t len;

	len = ecs_strlen(s);
	retval = g_new(ecs_char, len + 1);
	memcpy(retval, s, (len + 1) * sizeof(ecs_char));
	return retval;
}

size_t
ecs_strlcpy(ecs_char *dst, const ecs_char *src, size_t dstsize)
{
	size_t n = 0;		/* number of copied characters */

	if (dstsize >= 1) {
		while (n < dstsize - 1 && *src != ECS_CHAR('\0')) {
			*dst++ = *src++;
			n++;
		}
		*dst = ECS_CHAR('\0');
	}

	while (*src != ECS_CHAR('\0')) {
		n++;
		src++;
	}

	return n;
}

size_t
ecs_strlcat(ecs_char *dst, const ecs_char *src, size_t dstsize)
{
	size_t di = 0;

	while (di < dstsize && dst[di] != ECS_CHAR('\0'))
		di++;
	return di + ecs_strlcpy(dst + di, src, dstsize - di);
}

gboolean
ecs_strbox(const ecs_char *s, size_t *ret_width, size_t *ret_height)
{
	size_t nlines = 0, ncolumns = 0, colindex = 0, i;

	for (i = 0; s[i] != ECS_CHAR('\0'); i++) {
		if (s[i] == ECS_CHAR('\n')) {
			nlines++;
			colindex = 0;
		} else {
			if (!ecs_isprint(s[i]))
				return FALSE;

			/* FIXME: This code assumes that each printable
			 * character occupies one cell on the screen. */
			colindex++;
			if (colindex > ncolumns)
				ncolumns = colindex;
		}
	}
	*ret_width  = ncolumns;
	*ret_height = nlines;
	return TRUE;
}
