/*
   A parser for file-listings formatted like 'ls -l'.

   Copyright (C) 2016-2024
   Free Software Foundation, Inc.

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

/** \file
 *  \brief A parser for file-listings formatted like 'ls -l'.
 *
 * This program parses file-listings the same way MC does.
 * It's basically a wrapper around vfs_parse_ls_lga().
 * You can feed it the output of any of extfs's helpers.
 *
 * After parsing, the data is written out to stdout. The output
 * format can be either YAML or a format similar to 'ls -l'.
 *
 * This program is the main tool our tester script is going to use.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "lib/global.h"

#include "lib/vfs/utilvfs.h"    /* vfs_parse_ls_lga() */
#include "lib/util.h"           /* string_perm() */
#include "lib/timefmt.h"        /* FMT_LOCALTIME */
#include "lib/widget.h"         /* for the prototype of message() only */

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

typedef enum
{
    FORMAT_YAML,
    FORMAT_LS
} output_format_t;

/*** forward declarations (file scope functions) *************************************************/

static gboolean
parse_format_name_argument (const gchar * option_name, const gchar * value, gpointer data,
                            GError ** error);

/*** file scope variables ************************************************************************/

/* Command-line options. */
static gboolean opt_drop_mtime = FALSE;
static gboolean opt_drop_ids = FALSE;
static gboolean opt_symbolic_ids = FALSE;
static output_format_t opt_output_format = FORMAT_LS;

/* Misc. */
static int error_count = 0;

static GOptionEntry entries[] = {
    {"drop-mtime", 0, 0, G_OPTION_ARG_NONE, &opt_drop_mtime, "Don't include mtime in the output.",
     NULL},
    {"drop-ids", 0, 0, G_OPTION_ARG_NONE, &opt_drop_ids, "Don't include uid/gid in the output.",
     NULL},
    {"symbolic-ids", 0, 0, G_OPTION_ARG_NONE, &opt_symbolic_ids,
     "Print the strings '<<uid>>'/'<<gid>>' instead of the numeric IDs when they match the process' uid/gid.",
     NULL},
    {"format", 'f', 0, G_OPTION_ARG_CALLBACK, parse_format_name_argument,
     "Output format. Default: ls.", "<ls|yaml>"},
    G_OPTION_ENTRY_NULL
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 * Command-line handling.
 */
/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_format_name_argument (const gchar * option_name, const gchar * value, gpointer data,
                            GError ** error)
{
    (void) option_name;
    (void) data;

    if (strcmp (value, "yaml") == 0)
        opt_output_format = FORMAT_YAML;
    else if (strcmp (value, "ls") == 0)
        opt_output_format = FORMAT_LS;
    else
    {
        g_set_error (error, MC_ERROR, G_OPTION_ERROR_FAILED, "unknown output format '%s'", value);
        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_command_line (int *argc, char **argv[])
{
    GError *error = NULL;
    GOptionContext *context;

    context =
        g_option_context_new
        ("- Parses its input, which is expected to be in a format similar to 'ls -l', and writes the result to stdout.");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, argc, argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        g_error_free (error);

        return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Utility functions.
 */
/* --------------------------------------------------------------------------------------------- */

static const char *
my_itoa (int i)
{
    static char buf[BUF_SMALL];

    sprintf (buf, "%d", i);
    return buf;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Returns the uid as-is, or as "<<uid>>" if the same as current user.
 */
static const char *
symbolic_uid (uid_t uid)
{
    return (opt_symbolic_ids && uid == getuid ())? "<<uid>>" : my_itoa ((int) uid);
}

/* --------------------------------------------------------------------------------------------- */

static const char *
symbolic_gid (gid_t gid)
{
    return (opt_symbolic_ids && gid == getgid ())? "<<gid>>" : my_itoa ((int) gid);
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Cuts off a string's line-end (as in Perl).
 */
static void
chomp (char *s)
{
    int i;

    i = strlen (s);

    /* Code taken from vfs_parse_ls_lga(), with modifications. */
    if ((--i >= 0) && (s[i] == '\r' || s[i] == '\n'))
        s[i] = '\0';
    if ((--i >= 0) && (s[i] == '\r' || s[i] == '\n'))
        s[i] = '\0';
}

/* --------------------------------------------------------------------------------------------- */

static const char *
string_date (time_t t)
{
    static char buf[BUF_SMALL];

    /*
     * If we ever want to be able to re-parse the output of this program,
     * we'll need to use the American brain-damaged MM-DD-YYYY instead of
     * YYYY-MM-DD because vfs_parse_ls_lga() doesn't currently recognize
     * the latter.
     */
    FMT_LOCALTIME (buf, sizeof buf, "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Override MC's message().
 *
 * vfs_parse_ls_lga() calls this on error. Since MC's uses tty/widgets, it
 * will crash us. We replace it with a plain version.
 */
void
message (int flags, const char *title, const char *text, ...)
{
    char *p;
    va_list ap;

    (void) flags;
    (void) title;

    va_start (ap, text);
    p = g_strdup_vprintf (text, ap);
    va_end (ap);
    printf ("message(): vfs_parse_ls_lga(): parsing error at: %s\n", p);
    g_free (p);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * YAML output format.
 */
/* --------------------------------------------------------------------------------------------- */

static void
yaml_dump_stbuf (const struct stat *st)
{
    /* Various casts and flags here were taken/inspired by info_show_info(). */
    printf ("  perm: %s\n", string_perm (st->st_mode));
    if (!opt_drop_ids)
    {
        printf ("  uid: %s\n", symbolic_uid (st->st_uid));
        printf ("  gid: %s\n", symbolic_gid (st->st_gid));
    }
    printf ("  size: %" PRIuMAX "\n", (uintmax_t) st->st_size);
    printf ("  nlink: %d\n", (int) st->st_nlink);
    if (!opt_drop_mtime)
        printf ("  mtime: %s\n", string_date (st->st_mtime));
}

/* --------------------------------------------------------------------------------------------- */

static void
yaml_dump_string (const char *name, const char *val)
{
    char *q;

    q = g_shell_quote (val);
    printf ("  %s: %s\n", name, q);
    g_free (q);
}

/* --------------------------------------------------------------------------------------------- */

static void
yaml_dump_record (gboolean success, const char *input_line, const struct stat *st,
                  const char *filename, const char *linkname)
{
    printf ("-\n");             /* Start new item in the list. */

    if (success)
    {
        if (filename != NULL)
            yaml_dump_string ("name", filename);
        if (linkname != NULL)
            yaml_dump_string ("linkname", linkname);
        yaml_dump_stbuf (st);
    }
    else
    {
        yaml_dump_string ("cannot parse input line", input_line);
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * 'ls' output format.
 */
/* --------------------------------------------------------------------------------------------- */

static void
ls_dump_stbuf (const struct stat *st)
{
    /* Various casts and flags here were taken/inspired by info_show_info(). */
    printf ("%s %3d ", string_perm (st->st_mode), (int) st->st_nlink);
    if (!opt_drop_ids)
    {
        printf ("%8s ", symbolic_uid (st->st_uid));
        printf ("%8s ", symbolic_gid (st->st_gid));
    }
    printf ("%10" PRIuMAX " ", (uintmax_t) st->st_size);
    if (!opt_drop_mtime)
        printf ("%s ", string_date (st->st_mtime));
}

/* --------------------------------------------------------------------------------------------- */

static void
ls_dump_record (gboolean success, const char *input_line, const struct stat *st,
                const char *filename, const char *linkname)
{
    if (success)
    {
        ls_dump_stbuf (st);
        if (filename != NULL)
            printf ("%s", filename);
        if (linkname != NULL)
            printf (" -> %s", linkname);
        printf ("\n");
    }
    else
    {
        printf ("cannot parse input line: '%s'\n", input_line);
    }
}

/* ------------------------------------------------------------------------------ */
/**
 * Main code.
 */
/* ------------------------------------------------------------------------------ */

static void
process_ls_line (const char *line)
{
    struct stat st;
    char *filename, *linkname;
    gboolean success;

    memset (&st, 0, sizeof st);
    filename = NULL;
    linkname = NULL;

    success = vfs_parse_ls_lga (line, &st, &filename, &linkname, NULL);

    if (!success)
        error_count++;

    if (opt_output_format == FORMAT_YAML)
        yaml_dump_record (success, line, &st, filename, linkname);
    else
        ls_dump_record (success, line, &st, filename, linkname);

    g_free (filename);
    g_free (linkname);
}

/* ------------------------------------------------------------------------------ */

static void
process_input (FILE * input)
{
    char line[BUF_4K];

    while (fgets (line, sizeof line, input) != NULL)
    {
        chomp (line);           /* Not mandatory. Makes error messages nicer. */
        if (strncmp (line, "total ", 6) == 0)   /* Convenience only: makes 'ls -l' parse cleanly. */
            continue;
        process_ls_line (line);
    }
}

/* ------------------------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
    FILE *input;

    if (!parse_command_line (&argc, &argv))
        return EXIT_FAILURE;

    if (argc >= 2)
    {
        input = fopen (argv[1], "r");
        if (input == NULL)
        {
            perror (argv[1]);
            return EXIT_FAILURE;
        }
    }
    else
    {
        input = stdin;
    }

    process_input (input);

    return (error_count > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------------ */
