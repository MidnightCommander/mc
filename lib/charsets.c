/*
   Text conversion from one charset to another.

   Copyright (C) 2001-2024
   Free Software Foundation, Inc.

   Written by:
   Walery Studennikov <despair@sama.ru>

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file charsets.c
 *  \brief Source: Text conversion from one charset to another
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/global.h"
#include "lib/strutil.h"        /* utf-8 functions */
#include "lib/fileloc.h"
#include "lib/util.h"           /* whitespace() */

#include "lib/charsets.h"

/*** global variables ****************************************************************************/

GPtrArray *codepages = NULL;

unsigned char conv_displ[256];
unsigned char conv_input[256];

const char *cp_display = NULL;
const char *cp_source = NULL;

/*** file scope macro definitions ****************************************************************/

#define UNKNCHAR '\001'

#define OTHER_8BIT "Other_8_bit"

/*** file scope type declarations ****************************************************************/

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static codepage_desc *
new_codepage_desc (const char *id, const char *name)
{
    codepage_desc *desc;

    desc = g_new (codepage_desc, 1);
    desc->id = g_strdup (id);
    desc->name = g_strdup (name);

    return desc;
}

/* --------------------------------------------------------------------------------------------- */

static void
free_codepage_desc (gpointer data)
{
    codepage_desc *desc = (codepage_desc *) data;

    g_free (desc->id);
    g_free (desc->name);
    g_free (desc);
}

/* --------------------------------------------------------------------------------------------- */
/* returns display codepage */

static void
load_codepages_list_from_file (GPtrArray ** list, const char *fname)
{
    FILE *f;
    char buf[BUF_MEDIUM];
    char *default_codepage = NULL;

    f = fopen (fname, "r");
    if (f == NULL)
        return;

    while (fgets (buf, sizeof buf, f) != NULL)
    {
        /* split string into id and cpname */
        char *p = buf;
        size_t buflen;

        if (*p == '\n' || *p == '\0' || *p == '#')
            continue;

        buflen = strlen (buf);

        if (buflen != 0 && buf[buflen - 1] == '\n')
            buf[buflen - 1] = '\0';
        while (*p != '\0' && !whitespace (*p))
            ++p;
        if (*p == '\0')
            goto fail;

        *p++ = '\0';
        g_strstrip (p);
        if (*p == '\0')
            goto fail;

        if (strcmp (buf, "default") == 0)
            default_codepage = g_strdup (p);
        else
        {
            const char *id = buf;

            if (*list == NULL)
            {
                *list = g_ptr_array_sized_new (16);
                g_ptr_array_set_free_func (*list, free_codepage_desc);
                g_ptr_array_add (*list, new_codepage_desc (id, p));
            }
            else
            {
                unsigned int i;

                /* whether id is already present in list */
                /* if yes, overwrite description */
                for (i = 0; i < (*list)->len; i++)
                {
                    codepage_desc *desc;

                    desc = (codepage_desc *) g_ptr_array_index (*list, i);

                    if (strcmp (id, desc->id) == 0)
                    {
                        /* found */
                        g_free (desc->name);
                        desc->name = g_strdup (p);
                        break;
                    }
                }

                /* not found */
                if (i == (*list)->len)
                    g_ptr_array_add (*list, new_codepage_desc (id, p));
            }
        }
    }

    if (default_codepage != NULL)
    {
        mc_global.display_codepage = get_codepage_index (default_codepage);
        g_free (default_codepage);
    }

  fail:
    fclose (f);
}

/* --------------------------------------------------------------------------------------------- */

static char
translate_character (GIConv cd, char c)
{
    gchar *tmp_buff = NULL;
    gsize bytes_read, bytes_written = 0;
    const char *ibuf = &c;
    char ch = UNKNCHAR;
    int ibuflen = 1;

    tmp_buff = g_convert_with_iconv (ibuf, ibuflen, cd, &bytes_read, &bytes_written, NULL);
    if (tmp_buff != NULL)
        ch = tmp_buff[0];
    g_free (tmp_buff);
    return ch;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
load_codepages_list (void)
{
    char *fname;

    /* 1: try load /usr/share/mc/mc.charsets */
    fname = g_build_filename (mc_global.share_data_dir, CHARSETS_LIST, (char *) NULL);
    load_codepages_list_from_file (&codepages, fname);
    g_free (fname);

    /* 2: try load /etc/mc/mc.charsets */
    fname = g_build_filename (mc_global.sysconfig_dir, CHARSETS_LIST, (char *) NULL);
    load_codepages_list_from_file (&codepages, fname);
    g_free (fname);

    if (codepages == NULL)
    {
        /* files are not found, add default codepage */
        fprintf (stderr, "%s\n", _("Warning: cannot load codepages list"));

        codepages = g_ptr_array_new_with_free_func (free_codepage_desc);
        g_ptr_array_add (codepages, new_codepage_desc (DEFAULT_CHARSET, _("7-bit ASCII")));
    }
}

/* --------------------------------------------------------------------------------------------- */

void
free_codepages_list (void)
{
    g_ptr_array_free (codepages, TRUE);
    /* NULL-ize pointer to make unit tests happy */
    codepages = NULL;
}

/* --------------------------------------------------------------------------------------------- */

const char *
get_codepage_id (const int n)
{
    return (n < 0) ? OTHER_8BIT : ((codepage_desc *) g_ptr_array_index (codepages, n))->id;
}

/* --------------------------------------------------------------------------------------------- */

int
get_codepage_index (const char *id)
{
    size_t i;

    if (codepages == NULL)
        return -1;
    if (strcmp (id, OTHER_8BIT) == 0)
        return -1;
    for (i = 0; i < codepages->len; i++)
        if (strcmp (id, ((codepage_desc *) g_ptr_array_index (codepages, i))->id) == 0)
            return i;
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/** Check if specified encoding can be used in mc.
 * @param encoding name of encoding
 * @return TRUE if encoding is supported by mc, FALSE otherwise
 */

gboolean
is_supported_encoding (const char *encoding)
{
    gboolean result = FALSE;
    guint t;

    for (t = 0; t < codepages->len; t++)
    {
        const char *id;

        id = ((codepage_desc *) g_ptr_array_index (codepages, t))->id;
        result |= (g_ascii_strncasecmp (encoding, id, strlen (id)) == 0);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

char *
init_translation_table (int cpsource, int cpdisplay)
{
    int i;
    GIConv cd;

    /* Fill inpit <-> display tables */

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay)
    {
        for (i = 0; i <= 255; ++i)
        {
            conv_displ[i] = i;
            conv_input[i] = i;
        }
        cp_source = cp_display;
        return NULL;
    }

    for (i = 0; i <= 127; ++i)
    {
        conv_displ[i] = i;
        conv_input[i] = i;
    }
    cp_source = ((codepage_desc *) g_ptr_array_index (codepages, cpsource))->id;
    cp_display = ((codepage_desc *) g_ptr_array_index (codepages, cpdisplay))->id;

    /* display <- inpit table */

    cd = g_iconv_open (cp_display, cp_source);
    if (cd == INVALID_CONV)
        return g_strdup_printf (_("Cannot translate from %s to %s"), cp_source, cp_display);

    for (i = 128; i <= 255; ++i)
        conv_displ[i] = translate_character (cd, i);

    g_iconv_close (cd);

    /* inpit <- display table */

    cd = g_iconv_open (cp_source, cp_display);
    if (cd == INVALID_CONV)
        return g_strdup_printf (_("Cannot translate from %s to %s"), cp_display, cp_source);

    for (i = 128; i <= 255; ++i)
    {
        unsigned char ch;
        ch = translate_character (cd, i);
        conv_input[i] = (ch == UNKNCHAR) ? i : ch;
    }

    g_iconv_close (cd);

    return NULL;
}

/* --------------------------------------------------------------------------------------------- */

void
convert_to_display (char *str)
{
    if (str != NULL)
        for (; *str != '\0'; str++)
            *str = conv_displ[(unsigned char) *str];
}

/* --------------------------------------------------------------------------------------------- */

GString *
str_nconvert_to_display (const char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (str == NULL)
        return NULL;

    if (cp_display == cp_source)
        return g_string_new (str);

    conv = str_crt_conv_from (cp_source);

    buff = g_string_new ("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

/* --------------------------------------------------------------------------------------------- */

void
convert_from_input (char *str)
{
    if (str != NULL)
        for (; *str != '\0'; str++)
            *str = conv_input[(unsigned char) *str];
}

/* --------------------------------------------------------------------------------------------- */

GString *
str_nconvert_to_input (const char *str, int len)
{
    GString *buff;
    GIConv conv;

    if (str == NULL)
        return NULL;

    if (cp_display == cp_source)
        return g_string_new (str);

    conv = str_crt_conv_to (cp_source);

    buff = g_string_new ("");
    str_nconvert (conv, str, len, buff);
    str_close_conv (conv);
    return buff;
}

/* --------------------------------------------------------------------------------------------- */

unsigned char
convert_from_utf_to_current (const char *str)
{
    unsigned char buf_ch[UTF8_CHAR_LEN + 1];
    unsigned char ch = '.';
    GIConv conv;
    const char *cp_to;

    if (str == NULL)
        return '.';

    cp_to = get_codepage_id (mc_global.source_codepage);
    conv = str_crt_conv_to (cp_to);

    if (conv != INVALID_CONV)
    {
        switch (str_translate_char (conv, str, -1, (char *) buf_ch, sizeof (buf_ch)))
        {
        case ESTR_SUCCESS:
            ch = buf_ch[0];
            break;
        case ESTR_PROBLEM:
        case ESTR_FAILURE:
            ch = '.';
            break;
        default:
            break;
        }
        str_close_conv (conv);
    }

    return ch;
}

/* --------------------------------------------------------------------------------------------- */

unsigned char
convert_from_utf_to_current_c (int input_char, GIConv conv)
{
    unsigned char str[UTF8_CHAR_LEN + 1];
    unsigned char buf_ch[UTF8_CHAR_LEN + 1];
    unsigned char ch = '.';
    int res;

    res = g_unichar_to_utf8 (input_char, (char *) str);
    if (res == 0)
        return ch;

    str[res] = '\0';

    switch (str_translate_char (conv, (char *) str, -1, (char *) buf_ch, sizeof (buf_ch)))
    {
    case ESTR_SUCCESS:
        ch = buf_ch[0];
        break;
    case ESTR_PROBLEM:
    case ESTR_FAILURE:
        ch = '.';
        break;
    default:
        break;
    }

    return ch;
}

/* --------------------------------------------------------------------------------------------- */

int
convert_from_8bit_to_utf_c (char input_char, GIConv conv)
{
    unsigned char str[2];
    unsigned char buf_ch[UTF8_CHAR_LEN + 1];
    int ch;

    str[0] = (unsigned char) input_char;
    str[1] = '\0';

    switch (str_translate_char (conv, (char *) str, -1, (char *) buf_ch, sizeof (buf_ch)))
    {
    case ESTR_SUCCESS:
        {
            int res;

            res = g_utf8_get_char_validated ((char *) buf_ch, -1);
            ch = res >= 0 ? res : buf_ch[0];
            break;
        }
    case ESTR_PROBLEM:
    case ESTR_FAILURE:
    default:
        ch = '.';
        break;
    }

    return ch;
}

/* --------------------------------------------------------------------------------------------- */

int
convert_from_8bit_to_utf_c2 (char input_char)
{
    int ch = '.';
    GIConv conv;
    const char *cp_from;

    cp_from = get_codepage_id (mc_global.source_codepage);

    conv = str_crt_conv_to (cp_from);
    if (conv != INVALID_CONV)
    {
        ch = convert_from_8bit_to_utf_c (input_char, conv);
        str_close_conv (conv);
    }

    return ch;
}

/* --------------------------------------------------------------------------------------------- */
