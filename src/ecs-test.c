/*
   Testsuite for basic support for extended character sets.

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

/** \file  ecs-test.c
 *  \brief Source: testsuite for basic support for extended character sets
 */

#include <config.h>

#undef NDEBUG
#include <assert.h>
#include <locale.h>
#include <stdio.h>

#include "global.h"
#include "ecs.h"

#ifdef EXTCHARSET_ENABLED
static gboolean
change_locale(const char *loc)
{
	const char *ident;

	ident = setlocale(LC_CTYPE, loc);
	if (!ident) {
		(void) printf("Skipping %s locale\n", loc);
		return FALSE;
	} else {
		(void) printf("Testing %s locale \"%s\"\n", loc, ident);
		return TRUE;
	}
}

static void
test_locale_C(void)
{
	if (!change_locale("C")) return;

	assert(ecs_strlen(ECS_STR("foo")) == 3);
	assert(ecs_strlen(ECS_STR("Zuckert\374te")) == 10);
}

static void
test_locale_en_US_UTF_8(void)
{
	const char     *teststr_mb  = "Zuckert\303\214te";
	const ecs_char *teststr_ecs = ECS_STR("Zuckert\374te");
	const char     *teststr_c   = "Zuckert\374te";
	ecs_char       *ecs;
	char           *mbs;
	gboolean        valid;

	if (!change_locale("en_US.UTF-8")) return;

	valid = ecs_mbstr_to_str(&ecs, teststr_c);
	assert(!valid);

	valid = ecs_mbstr_to_str(&ecs, teststr_mb);
	assert(valid);
	assert(ecs_strlen(ecs) == 10);
	g_free(ecs);

	valid = ecs_str_to_mbstr(&mbs, teststr_ecs);
	assert(valid);
	assert(strlen(mbs) == 11);
	g_free(mbs);
}
#endif

/* ecs_strcpy */
/* ecs_strncpy */
/* ecs_strcat */
/* ecs_strncat */

static void
test_ecs_strcmp(void)
{
	/* This test assumes ASCII encoding */

	(void) puts("Testing ecs_strcmp ...");
	assert(ecs_strcmp(ECS_STR("foo"), ECS_STR("bar")) > 0);
	assert(ecs_strcmp(ECS_STR("bar"), ECS_STR("foo")) < 0);
	assert(ecs_strcmp(ECS_STR(""), ECS_STR("")) == 0);
	assert(ecs_strcmp(ECS_STR("f"), ECS_STR("")) > 0);
	assert(ecs_strcmp(ECS_STR(""), ECS_STR("f")) < 0);
}

/* ecs_strcoll */
/* ecs_strncmp */
/* ecs_strxfrm */

static void
test_ecs_strchr(void)
{
	const ecs_char foo[] = ECS_STR("foo");

	(void) puts("Testing ecs_strchr ...");
	assert(ecs_strchr(foo, ECS_CHAR('f')) == foo);
	assert(ecs_strchr(foo, ECS_CHAR('o')) == foo + 1);
	assert(ecs_strchr(foo, ECS_CHAR('\0')) == foo + 3);
	assert(ecs_strchr(foo, ECS_CHAR('b')) == NULL);
}

static void
test_ecs_strcspn(void)
{
	const ecs_char test[] = ECS_STR("test string0123");

	(void) puts("Testing ecs_strcspn ...");
	assert(ecs_strcspn(test, ECS_STR("t")) == 0);
	assert(ecs_strcspn(test, ECS_STR("e")) == 1);
	assert(ecs_strcspn(test, ECS_STR("te")) == 0);
	assert(ecs_strcspn(test, ECS_STR("et")) == 0);
	assert(ecs_strcspn(test, ECS_STR("")) == 15);
	assert(ecs_strcspn(test, ECS_STR("XXX")) == 15);
}

/* ecs_strpbrk */

static void
test_ecs_strrchr(void)
{
	const ecs_char foo[] = ECS_STR("foo");

	(void) puts("Testing ecs_strrchr ...");
	assert(ecs_strrchr(foo, ECS_CHAR('f')) == foo);
	assert(ecs_strrchr(foo, ECS_CHAR('o')) == foo + 2);
	assert(ecs_strrchr(foo, ECS_CHAR('\0')) == foo + 3);
	assert(ecs_strrchr(foo, ECS_CHAR('b')) == NULL);
}

/* extern ecs_char *ecs_strstr(const ecs_char *, const ecs_char *); */

/* ecs_strtok */

static void
test_ecs_strlen(void)
{
	(void) puts("Testing ecs_strlen ...");
	assert(ecs_strlen(ECS_STR("")) == 0);
	assert(ecs_strlen(ECS_STR("foo")) == 3);
	assert(ecs_strlen(ECS_STR("\1\2\3\4\5")) == 5);
}

/* extern ecs_char *ecs_xstrdup(const ecs_char *); */

static void
test_ecs_strlcpy(void)
{
	ecs_char dest[20];

	(void) puts("Testing ecs_strlcpy ...");
	assert(ecs_strlcpy(dest, ECS_STR(""), sizeof(dest)) == 0);
	assert(dest[0] == ECS_CHAR('\0'));
	assert(ecs_strlcpy(dest, ECS_STR("onetwothree"), sizeof(dest)) == 11);
	assert(dest[11] == ECS_CHAR('\0'));
	assert(ecs_strcmp(dest, ECS_STR("onetwothree")) == 0);
	assert(ecs_strlcpy(dest, ECS_STR("onetwothree"), 5) == 11);
	assert(dest[4] == ECS_CHAR('\0'));
	assert(ecs_strcmp(dest, ECS_STR("onet")) == 0);
}

static void
test_ecs_strlcat(void)
{
	ecs_char dest[20];

	(void) puts("Testing ecs_strlcat ...");
	dest[0] = ECS_CHAR('\0');
	assert(ecs_strlcat(dest, ECS_STR("foo"), 0) == 3);
	assert(dest[0] == ECS_CHAR('\0'));
	assert(ecs_strlcat(dest, ECS_STR("foo"), 1) == 3);
	assert(dest[0] == ECS_CHAR('\0'));
	assert(ecs_strlcat(dest, ECS_STR("foo"), 2) == 3);
	assert(dest[0] == ECS_CHAR('f'));
	assert(dest[1] == ECS_CHAR('\0'));
	dest[1] = ECS_CHAR('X');
	assert(ecs_strlcat(dest, ECS_STR("bar"), 1) == 4);
	assert(dest[0] == ECS_CHAR('f'));
	assert(dest[1] == ECS_CHAR('X'));
}

/* extern void ecs_strbox(const ecs_char *, size_t *ret_width,
	size_t *ret_height); */

int main(void)
{
#ifdef EXTCHARSET_ENABLED
	test_locale_C();
	test_locale_en_US_UTF_8();
#endif
	test_ecs_strcmp();
	test_ecs_strchr();
	test_ecs_strrchr();
	test_ecs_strlen();
	test_ecs_strlcpy();
	test_ecs_strlcat();
	test_ecs_strcspn();
	(void) puts("All tests passed.");
	return 0;
}
