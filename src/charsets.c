/* Text conversion from one charset to another.

   Copyright (C) 2001 Walery Studennikov <despair@sama.ru>

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#ifdef HAVE_CHARSET

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iconv.h>

#include <mhl/string.h>

#include "global.h"
#include "charsets.h"

int n_codepages = 0;

struct codepage_desc *codepages;

unsigned char conv_displ[256];
unsigned char conv_input[256];

int
load_codepages_list (void)
{
    int result = -1;
    FILE *f;
    char *fname;
    char buf[256];
    extern char *mc_home;
    extern int display_codepage;
    char *default_codepage = NULL;

    fname = mhl_str_dir_plus_file (mc_home, CHARSETS_INDEX);
    if (!(f = fopen (fname, "r"))) {
	fprintf (stderr, _("Warning: file %s not found\n"), fname);
	g_free (fname);
	return -1;
    }
    g_free (fname);

    for (n_codepages = 0; fgets (buf, sizeof (buf), f);)
	if (buf[0] != '\n' && buf[0] != '\0' && buf[0] != '#')
	    ++n_codepages;
    rewind (f);

    codepages = g_new0 (struct codepage_desc, n_codepages + 1);

    for (n_codepages = 0; fgets (buf, sizeof buf, f);) {
	/* split string into id and cpname */
	char *p = buf;
	int buflen = strlen (buf);

	if (*p == '\n' || *p == '\0' || *p == '#')
	    continue;

	if (buflen > 0 && buf[buflen - 1] == '\n')
	    buf[buflen - 1] = '\0';
	while (*p != '\t' && *p != ' ' && *p != '\0')
	    ++p;
	if (*p == '\0')
	    goto fail;

	*p++ = 0;

	while (*p == '\t' || *p == ' ')
	    ++p;
	if (*p == '\0')
	    goto fail;

	if (strcmp (buf, "default") == 0) {
	    default_codepage = g_strdup (p);
	    continue;
	}

	codepages[n_codepages].id = g_strdup (buf);
	codepages[n_codepages].name = g_strdup (p);
	++n_codepages;
    }

    if (default_codepage) {
	display_codepage = get_codepage_index (default_codepage);
	g_free (default_codepage);
    }

    result = n_codepages;
  fail:
    fclose (f);
    return result;
}

void
free_codepages_list (void)
{
    if (n_codepages > 0) {
	int i;
	for (i = 0; i < n_codepages; i++) {
	    g_free (codepages[i].id);
	    g_free (codepages[i].name);
	}
	n_codepages = 0;
	g_free (codepages);
	codepages = 0;
    }
}

#define OTHER_8BIT "Other_8_bit"

const char *
get_codepage_id (int n)
{
    return (n < 0) ? OTHER_8BIT : codepages[n].id;
}

int
get_codepage_index (const char *id)
{
    int i;
    if (strcmp (id, OTHER_8BIT) == 0)
	return -1;
    for (i = 0; codepages[i].id; ++i)
	if (strcmp (id, codepages[i].id) == 0)
	    return i;
    return -1;
}

static char
translate_character (iconv_t cd, char c)
{
    char outbuf[4], *obuf;
    size_t ibuflen, obuflen, count;

    ICONV_CONST char *ibuf = &c;
    obuf = outbuf;
    ibuflen = 1;
    obuflen = 4;

    count = iconv (cd, &ibuf, &ibuflen, &obuf, &obuflen);
    if (count != ((size_t) -1) && ibuflen == 0)
	return outbuf[0];

    return UNKNCHAR;
}

char errbuf[255];

/*
 * FIXME: This assumes that ASCII is always the first encoding
 * in mc.charsets
 */
#define CP_ASCII 0

const char *
init_translation_table (int cpsource, int cpdisplay)
{
    int i;
    iconv_t cd;
    const char *cpsour, *cpdisp;

    /* Fill inpit <-> display tables */

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay) {
	for (i = 0; i <= 255; ++i) {
	    conv_displ[i] = i;
	    conv_input[i] = i;
	}
	return NULL;
    }

    for (i = 0; i <= 127; ++i) {
	conv_displ[i] = i;
	conv_input[i] = i;
    }

    cpsour = codepages[cpsource].id;
    cpdisp = codepages[cpdisplay].id;

    /* display <- inpit table */

    cd = iconv_open (cpdisp, cpsour);
    if (cd == (iconv_t) - 1) {
	g_snprintf (errbuf, sizeof (errbuf),
		    _("Cannot translate from %s to %s"), cpsour, cpdisp);
	return errbuf;
    }

    for (i = 128; i <= 255; ++i)
	conv_displ[i] = translate_character (cd, i);

    iconv_close (cd);

    /* inpit <- display table */

    cd = iconv_open (cpsour, cpdisp);
    if (cd == (iconv_t) - 1) {
	g_snprintf (errbuf, sizeof (errbuf),
		    _("Cannot translate from %s to %s"), cpdisp, cpsour);
	return errbuf;
    }

    for (i = 128; i <= 255; ++i) {
	unsigned char ch;
	ch = translate_character (cd, i);
	conv_input[i] = (ch == UNKNCHAR) ? i : ch;
    }

    iconv_close (cd);

    return NULL;
}

void
convert_to_display (char *str)
{
    if (!str)
	return;

    while (*str) {
	*str = conv_displ[(unsigned char) *str];
	str++;
    }
}

void
convert_from_input (char *str)
{
    if (!str)
	return;

    while (*str) {
	*str = conv_input[(unsigned char) *str];
	str++;
    }
}
#endif				/* HAVE_CHARSET */
