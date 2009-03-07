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

/** \file  ecs.h
 *  \brief Header: basic support for extended character sets
 */

#ifndef MC_ECS_H
#define MC_ECS_H

/*
 * This header provides string processing functions for extended
 * character sets (ECS), as well as for the traditional one-to-one
 * byte-to-character encoding.
 */

#include <sys/types.h>		/* size_t */

#include "global.h"		/* include <glib.h> */

/* Use the macros ECS_CHAR and ECS_STR to bring character and string
 * literals to the correct form required by the C compiler. */
#ifdef EXTCHARSET_ENABLED
#  include <stdlib.h>
typedef wchar_t ecs_char;
#  define ECS_CHAR(c)		(L##c)
#  define ECS_STR(s)		(L##s)
#else
typedef char ecs_char;
#  define ECS_CHAR(c)		(c)
#  define ECS_STR(s)		(s)
#endif

/*
 * String conversion functions between the wide character encoding and
 * the multibyte encoding. The returned strings should be freed using
 * g_free after use. The return value is TRUE if the string is valid
 * and has been converted, FALSE otherwise.
 */

extern gboolean ecs_mbstr_to_str(ecs_char **ret_str, const char *);
extern gboolean ecs_str_to_mbstr(char **ret_str, const ecs_char *);

/*
 * Replacements for the ISO C90 <ctype.h> functions.
 */

extern gboolean ecs_isalnum(ecs_char);
extern gboolean ecs_isalpha(ecs_char);
extern gboolean ecs_iscntrl(ecs_char);
extern gboolean ecs_isdigit(ecs_char);
extern gboolean ecs_isgraph(ecs_char);
extern gboolean ecs_islower(ecs_char);
extern gboolean ecs_isprint(ecs_char);
extern gboolean ecs_ispunct(ecs_char);
extern gboolean ecs_isspace(ecs_char);
extern gboolean ecs_isupper(ecs_char);
extern gboolean ecs_isxdigit(ecs_char);

/*
 * Replacements for the ISO C90 <string.h> functions.
 */

/* left out: ecs_strcpy */
/* left out: ecs_strncpy */
/* left out: ecs_strcat */
/* left out: ecs_strncat */
extern int ecs_strcmp(const ecs_char *, const ecs_char *);
/* left out: ecs_strcoll */
/* left out: ecs_strncmp */
/* left out: ecs_strxfrm */
extern ecs_char *ecs_strchr(const ecs_char *, ecs_char);
extern size_t ecs_strcspn(const ecs_char *, const ecs_char *);
/* left out: ecs_strpbrk */
extern ecs_char *ecs_strrchr(const ecs_char *, ecs_char);
extern size_t ecs_strspn(const ecs_char *, const ecs_char *);
extern ecs_char *ecs_strstr(const ecs_char *, const ecs_char *);
/* left out: ecs_strtok */
extern size_t ecs_strlen(const ecs_char *);

/*
 * Other string functions.
 */

/* allocates a copy of the string. Never returns NULL. */
extern ecs_char *ecs_xstrdup(const ecs_char *);

extern size_t ecs_strlcpy(ecs_char *, const ecs_char *, size_t);
extern size_t ecs_strlcat(ecs_char *, const ecs_char *, size_t);

/* calculates the bounds of the box that the string would occupy when
 * displayed on screen. Returns TRUE if all characters in the string are
 * either '\n' or printable, according to the current locale. If the
 * return value is FALSE, ''width'' and ''height'' are not modified. */
extern gboolean ecs_strbox(const ecs_char *, size_t *ret_width,
	size_t *ret_height);

#endif
