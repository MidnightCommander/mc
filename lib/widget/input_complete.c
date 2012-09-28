/*
   Input line filename/username/hostname/variable/command completion.
   (Let mc type for you...)

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2007, 2011
   the Free Software Foundation, Inc.

   Written by:
   Jakub Jelinek, 1995

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

/** \file lib/widget/input_complete.c
 *  \brief Source: Input line filename/username/hostname/variable/command completion
 */

#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* XCTRL and ALT macros */
#include "lib/vfs/vfs.h"
#include "lib/strescape.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "input_complete.h"

/*** global variables ****************************************************************************/

/* Linux declares environ in <unistd.h>, so don't repeat it here. */
#if (!(defined(__linux__) && defined (__USE_GNU)) && !defined(__CYGWIN__))
extern char **environ;
#endif

/*** file scope macro definitions ****************************************************************/

/* #define DO_COMPLETION_DEBUG */
#ifdef DO_COMPLETION_DEBUG
#define SHOW_C_CTX(func) fprintf(stderr, "%s: text='%s' flags=%s\n", func, text, show_c_flags(flags))
#else
#define SHOW_C_CTX(func)
#endif /* DO_CMPLETION_DEBUG */

#define whitespace(c) ((c) == ' ' || (c) == '\t')
#define cr_whitespace(c) (whitespace (c) || (c) == '\n' || (c) == '\r')

#define DO_INSERTION 1
#define DO_QUERY     2

/*** file scope type declarations ****************************************************************/

typedef char *CompletionFunction (const char *text, int state, input_complete_t flags);

/*** file scope variables ************************************************************************/

static char **hosts = NULL;
static char **hosts_p = NULL;
static int hosts_alloclen = 0;

static int query_height, query_width;
static WInput *input;
static int min_end;
static int start = 0;
static int end = 0;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef DO_COMPLETION_DEBUG
/**
 * Useful to print/debug completion flags
 */
static const char *
show_c_flags (input_complete_t flags)
{
    static char s_cf[] = "FHCVUDS";

    s_cf[0] = (flags & INPUT_COMPLETE_FILENAMES) ? 'F' : ' ';
    s_cf[1] = (flags & INPUT_COMPLETE_HOSTNAMES) ? 'H' : ' ';
    s_cf[2] = (flags & INPUT_COMPLETE_COMMANDS) ? 'C' : ' ';
    s_cf[3] = (flags & INPUT_COMPLETE_VARIABLES) ? 'V' : ' ';
    s_cf[4] = (flags & INPUT_COMPLETE_USERNAMES) ? 'U' : ' ';
    s_cf[5] = (flags & INPUT_COMPLETE_CD) ? 'D' : ' ';
    s_cf[6] = (flags & INPUT_COMPLETE_SHELL_ESC) ? 'S' : ' ';

    return s_cf;
}
#endif /* DO_CMPLETION_DEBUG */

/* --------------------------------------------------------------------------------------------- */

static char *
filename_completion_function (const char *text, int state, input_complete_t flags)
{
    static DIR *directory = NULL;
    static char *filename = NULL;
    static char *dirname = NULL;
    static char *users_dirname = NULL;
    static size_t filename_len;
    int isdir = 1, isexec = 0;
    static vfs_path_t *dirname_vpath = NULL;

    struct dirent *entry = NULL;

    SHOW_C_CTX ("filename_completion_function");

    if (text && (flags & INPUT_COMPLETE_SHELL_ESC))
    {
        char *u_text;
        char *result;
        char *e_result;

        u_text = strutils_shell_unescape (text);

        result = filename_completion_function (u_text, state, flags & (~INPUT_COMPLETE_SHELL_ESC));
        g_free (u_text);

        e_result = strutils_shell_escape (result);
        g_free (result);

        return e_result;
    }

    /* If we're starting the match process, initialize us a bit. */
    if (state == 0)
    {
        const char *temp;

        g_free (dirname);
        g_free (filename);
        g_free (users_dirname);
        vfs_path_free (dirname_vpath);

        if ((*text != '\0') && (temp = strrchr (text, PATH_SEP)) != NULL)
        {
            filename = g_strdup (++temp);
            dirname = g_strndup (text, temp - text);
        }
        else
        {
            dirname = g_strdup (".");
            filename = g_strdup (text);
        }

        /* We aren't done yet.  We also support the "~user" syntax. */

        /* Save the version of the directory that the user typed. */
        users_dirname = dirname;
        dirname = tilde_expand (dirname);
        canonicalize_pathname (dirname);
        dirname_vpath = vfs_path_from_str (dirname);

        /* Here we should do something with variable expansion
           and `command`.
           Maybe a dream - UNIMPLEMENTED yet. */

        directory = mc_opendir (dirname_vpath);
        filename_len = strlen (filename);
    }

    /* Now that we have some state, we can read the directory. */

    while (directory && (entry = mc_readdir (directory)))
    {
        if (!str_is_valid_string (entry->d_name))
            continue;

        /* Special case for no filename.
           All entries except "." and ".." match. */
        if (filename_len == 0)
        {
            if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, ".."))
                continue;
        }
        else
        {
            /* Otherwise, if these match up to the length of filename, then
               it may be a match. */
            if ((entry->d_name[0] != filename[0]) ||
                ((NLENGTH (entry)) < filename_len) ||
                strncmp (filename, entry->d_name, filename_len))
                continue;
        }
        isdir = 1;
        isexec = 0;
        {
            struct stat tempstat;
            vfs_path_t *tmp_vpath;

            tmp_vpath = vfs_path_build_filename (dirname, entry->d_name, (char *) NULL);

            /* Unix version */
            if (mc_stat (tmp_vpath, &tempstat) == 0)
            {
                uid_t my_uid = getuid ();
                gid_t my_gid = getgid ();

                if (!S_ISDIR (tempstat.st_mode))
                {
                    isdir = 0;
                    if ((!my_uid && (tempstat.st_mode & 0111)) ||
                        (my_uid == tempstat.st_uid && (tempstat.st_mode & 0100)) ||
                        (my_gid == tempstat.st_gid && (tempstat.st_mode & 0010)) ||
                        (tempstat.st_mode & 0001))
                        isexec = 1;
                }
            }
            else
            {
                /* stat failed, strange. not a dir in any case */
                isdir = 0;
            }
            vfs_path_free (tmp_vpath);
        }
        if ((flags & INPUT_COMPLETE_COMMANDS) && (isexec || isdir))
            break;
        if ((flags & INPUT_COMPLETE_CD) && isdir)
            break;
        if (flags & (INPUT_COMPLETE_FILENAMES))
            break;
    }

    if (entry == NULL)
    {
        if (directory)
        {
            mc_closedir (directory);
            directory = NULL;
        }
        g_free (dirname);
        dirname = NULL;
        vfs_path_free (dirname_vpath);
        dirname_vpath = NULL;
        g_free (filename);
        filename = NULL;
        g_free (users_dirname);
        users_dirname = NULL;
        return NULL;
    }
    else
    {
        char *temp;

        if (users_dirname && (users_dirname[0] != '.' || users_dirname[1]))
        {
            size_t dirlen = strlen (users_dirname);
            temp = g_malloc (3 + dirlen + NLENGTH (entry));
            strcpy (temp, users_dirname);
            /* We need a `/' at the end. */
            if (users_dirname[dirlen - 1] != PATH_SEP)
            {
                temp[dirlen] = PATH_SEP;
                temp[dirlen + 1] = '\0';
            }
            strcat (temp, entry->d_name);
        }
        else
        {
            temp = g_malloc (2 + NLENGTH (entry));
            strcpy (temp, entry->d_name);
        }
        if (isdir)
            strcat (temp, PATH_SEP_STR);

        return temp;
    }
}

/* --------------------------------------------------------------------------------------------- */
/** We assume here that text[0] == '~' , if you want to call it in another way,
   you have to change the code */

static char *
username_completion_function (const char *text, int state, input_complete_t flags)
{
    static struct passwd *entry;
    static size_t userlen;

    (void) flags;
    SHOW_C_CTX ("username_completion_function");

    if (text[0] == '\\' && text[1] == '~')
        text++;
    if (state == 0)
    {                           /* Initialization stuff */
        setpwent ();
        userlen = strlen (text + 1);
    }
    while ((entry = getpwent ()) != NULL)
    {
        /* Null usernames should result in all users as possible completions. */
        if (userlen == 0)
            break;
        if (text[1] == entry->pw_name[0] && !strncmp (text + 1, entry->pw_name, userlen))
            break;
    }

    if (entry != NULL)
        return g_strconcat ("~", entry->pw_name, PATH_SEP_STR, (char *) NULL);

    endpwent ();
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/** We assume text [0] == '$' and want to have a look at text [1], if it is
   equal to '{', so that we should append '}' at the end */

static char *
variable_completion_function (const char *text, int state, input_complete_t flags)
{
    static char **env_p;
    static int varlen, isbrace;
    const char *p = NULL;

    (void) flags;
    SHOW_C_CTX ("variable_completion_function");

    if (state == 0)
    {                           /* Initialization stuff */
        isbrace = (text[1] == '{');
        varlen = strlen (text + 1 + isbrace);
        env_p = environ;
    }

    while (*env_p)
    {
        p = strchr (*env_p, '=');
        if (p && p - *env_p >= varlen && !strncmp (text + 1 + isbrace, *env_p, varlen))
            break;
        env_p++;
    }

    if (*env_p == NULL)
        return NULL;

    {
        char *temp = g_malloc (2 + 2 * isbrace + p - *env_p);

        *temp = '$';
        if (isbrace)
            temp[1] = '{';
        memcpy (temp + 1 + isbrace, *env_p, p - *env_p);
        if (isbrace)
            strcpy (temp + 2 + (p - *env_p), "}");
        else
            temp[1 + p - *env_p] = 0;
        env_p++;
        return temp;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
fetch_hosts (const char *filename)
{
    FILE *file = fopen (filename, "r");
    char buffer[256], *name;
    char *lc_start;
    char *bi;

    if (!file)
        return;

    while (fgets (buffer, 255, file) != NULL)
    {
        /* Skip to first character. */
        for (bi = buffer; bi[0] != '\0' && str_isspace (bi); str_next_char (&bi));

        /* Ignore comments... */
        if (bi[0] == '#')
            continue;
        /* Handle $include. */
        if (!strncmp (bi, "$include ", 9))
        {
            char *includefile = bi + 9;
            char *t;

            /* Find start of filename. */
            while (*includefile && whitespace (*includefile))
                includefile++;
            t = includefile;

            /* Find end of filename. */
            while (t[0] != '\0' && !str_isspace (t))
                str_next_char (&t);
            *t = '\0';

            fetch_hosts (includefile);
            continue;
        }

        /* Skip IP #s. */
        while (bi[0] != '\0' && !str_isspace (bi))
            str_next_char (&bi);

        /* Get the host names separated by white space. */
        while (bi[0] != '\0' && bi[0] != '#')
        {
            while (bi[0] != '\0' && str_isspace (bi))
                str_next_char (&bi);
            if (bi[0] == '#')
                continue;
            for (lc_start = bi; bi[0] != '\0' && !str_isspace (bi); str_next_char (&bi));

            if (bi - lc_start == 0)
                continue;

            name = g_strndup (lc_start, bi - lc_start);
            {
                char **host_p;

                if (hosts_p - hosts >= hosts_alloclen)
                {
                    int j = hosts_p - hosts;

                    hosts =
                        g_realloc ((void *) hosts, ((hosts_alloclen += 30) + 1) * sizeof (char *));
                    hosts_p = hosts + j;
                }
                for (host_p = hosts; host_p < hosts_p; host_p++)
                    if (!strcmp (name, *host_p))
                        break;  /* We do not want any duplicates */
                if (host_p == hosts_p)
                {
                    *(hosts_p++) = name;
                    *hosts_p = NULL;
                }
                else
                    g_free (name);
            }
        }
    }
    fclose (file);
}

/* --------------------------------------------------------------------------------------------- */

static char *
hostname_completion_function (const char *text, int state, input_complete_t flags)
{
    static char **host_p;
    static int textstart, textlen;

    (void) flags;
    SHOW_C_CTX ("hostname_completion_function");

    if (!state)
    {                           /* Initialization stuff */
        const char *p;

        if (hosts != NULL)
        {
            for (host_p = hosts; *host_p; host_p++)
                g_free (*host_p);
            g_free (hosts);
        }
        hosts = g_new (char *, (hosts_alloclen = 30) + 1);
        *hosts = NULL;
        hosts_p = hosts;
        fetch_hosts ((p = getenv ("HOSTFILE")) ? p : "/etc/hosts");
        host_p = hosts;
        textstart = (*text == '@') ? 1 : 0;
        textlen = strlen (text + textstart);
    }

    while (*host_p)
    {
        if (!textlen)
            break;              /* Match all of them */
        else if (!strncmp (text + textstart, *host_p, textlen))
            break;
        host_p++;
    }

    if (!*host_p)
    {
        for (host_p = hosts; *host_p; host_p++)
            g_free (*host_p);
        g_free (hosts);
        hosts = NULL;
        return NULL;
    }
    else
    {
        char *temp = g_malloc (2 + strlen (*host_p));

        if (textstart)
            *temp = '@';
        strcpy (temp + textstart, *host_p);
        host_p++;
        return temp;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This is the function to call when the word to complete is in a position
 * where a command word can be found. It looks around $PATH, looking for
 * commands that match. It also scans aliases, function names, and the
 * table of shell built-ins.
 */

static char *
command_completion_function (const char *_text, int state, input_complete_t flags)
{
    char *text;
    static const char *path_end;
    static gboolean isabsolute;
    static int phase;
    static size_t text_len;
    static const char *const *words;
    static char *path;
    static char *cur_path;
    static char *cur_word;
    static int init_state;
    static const char *const bash_reserved[] = {
        "if", "then", "else", "elif", "fi", "case", "esac", "for",
        "select", "while", "until", "do", "done", "in", "function", 0
    };
    static const char *const bash_builtins[] = {
        "alias", "bg", "bind", "break", "builtin", "cd", "command",
        "continue", "declare", "dirs", "echo", "enable", "eval",
        "exec", "exit", "export", "fc", "fg", "getopts", "hash",
        "help", "history", "jobs", "kill", "let", "local", "logout",
        "popd", "pushd", "pwd", "read", "readonly", "return", "set",
        "shift", "source", "suspend", "test", "times", "trap", "type",
        "typeset", "ulimit", "umask", "unalias", "unset", "wait", 0
    };
    char *p, *found;

    SHOW_C_CTX ("command_completion_function");

    if (!(flags & INPUT_COMPLETE_COMMANDS))
        return 0;

    text = strutils_shell_unescape (_text);
    flags &= ~INPUT_COMPLETE_SHELL_ESC;

    if (state == 0)
    {                           /* Initialize us a little bit */
        isabsolute = strchr (text, PATH_SEP) != NULL;
        if (!isabsolute)
        {
            words = bash_reserved;
            phase = 0;
            text_len = strlen (text);

            if (path == NULL)
            {
                path = g_strdup (getenv ("PATH"));
                if (path != NULL)
                {
                    p = path;
                    path_end = strchr (p, '\0');
                    while ((p = strchr (p, PATH_ENV_SEP)) != NULL)
                    {
                        *p++ = '\0';
                    }
                }
            }
        }
    }

    if (isabsolute)
    {
        p = filename_completion_function (text, state, flags);

        if (p != NULL)
        {
            char *temp_p = p;
            p = strutils_shell_escape (p);
            g_free (temp_p);
        }

        g_free (text);
        return p;
    }

    found = NULL;
    switch (phase)
    {
    case 0:                    /* Reserved words */
        while (*words)
        {
            if (strncmp (*words, text, text_len) == 0)
            {
                g_free (text);
                return g_strdup (*(words++));
            }
            words++;
        }
        phase++;
        words = bash_builtins;
    case 1:                    /* Builtin commands */
        while (*words)
        {
            if (strncmp (*words, text, text_len) == 0)
            {
                g_free (text);
                return g_strdup (*(words++));
            }
            words++;
        }
        phase++;
        if (!path)
            break;
        cur_path = path;
        cur_word = NULL;
    case 2:                    /* And looking through the $PATH */
        while (!found)
        {
            if (!cur_word)
            {
                char *expanded;

                if (cur_path >= path_end)
                    break;
                expanded = tilde_expand (*cur_path ? cur_path : ".");
                cur_word = mc_build_filename (expanded, text, NULL);
                g_free (expanded);
                canonicalize_pathname (cur_word);
                cur_path = strchr (cur_path, 0) + 1;
                init_state = state;
            }
            found = filename_completion_function (cur_word, state - init_state, flags);
            if (!found)
            {
                g_free (cur_word);
                cur_word = NULL;
            }
        }
    }

    if (found == NULL)
    {
        g_free (path);
        path = NULL;
    }
    else
    {
        p = strrchr (found, PATH_SEP);
        if (p != NULL)
        {
            char *tmp = found;
            found = strutils_shell_escape (p + 1);
            g_free (tmp);
        }
    }

    g_free (text);
    return found;
}

/* --------------------------------------------------------------------------------------------- */

static int
match_compare (const void *a, const void *b)
{
    return strcmp (*(char **) a, *(char **) b);
}

/* --------------------------------------------------------------------------------------------- */
/** Returns an array of char * matches with the longest common denominator
   in the 1st entry. Then a NULL terminated list of different possible
   completions follows.
   You have to supply your own CompletionFunction with the word you
   want to complete as the first argument and an count of previous matches
   as the second. 
   In case no matches were found we return NULL. */

static char **
completion_matches (const char *text, CompletionFunction entry_function, input_complete_t flags)
{
    /* Number of slots in match_list. */
    int match_list_size;

    /* The list of matches. */
    char **match_list = g_new (char *, (match_list_size = 30) + 1);

    /* Number of matches actually found. */
    int matches = 0;

    /* Temporary string binder. */
    char *string;

    match_list[1] = NULL;

    while ((string = (*entry_function) (text, matches, flags)) != NULL)
    {
        if (matches + 1 == match_list_size)
            match_list =
                (char **) g_realloc (match_list, ((match_list_size += 30) + 1) * sizeof (char *));
        match_list[++matches] = string;
        match_list[matches + 1] = NULL;
    }

    /* If there were any matches, then look through them finding out the
       lowest common denominator.  That then becomes match_list[0]. */
    if (matches)
    {
        register int i = 1;
        int low = 4096;         /* Count of max-matched characters. */

        /* If only one match, just use that. */
        if (matches == 1)
        {
            match_list[0] = match_list[1];
            match_list[1] = NULL;
        }
        else
        {
            int j;

            qsort (match_list + 1, matches, sizeof (char *), match_compare);

            /* And compare each member of the list with
               the next, finding out where they stop matching. 
               If we find two equal strings, we have to put one away... */

            j = i + 1;
            while (j < matches + 1)
            {
                char *si, *sj;
                char *ni, *nj;

                for (si = match_list[i], sj = match_list[j]; si[0] && sj[0];)
                {

                    ni = str_get_next_char (si);
                    nj = str_get_next_char (sj);

                    if (ni - si != nj - sj)
                        break;
                    if (strncmp (si, sj, ni - si) != 0)
                        break;

                    si = ni;
                    sj = nj;
                }

                if (si[0] == '\0' && sj[0] == '\0')
                {               /* Two equal strings */
                    g_free (match_list[j]);
                    j++;
                    if (j > matches)
                        break;
                    continue;   /* Look for a run of equal strings */
                }
                else if (low > si - match_list[i])
                    low = si - match_list[i];
                if (i + 1 != j) /* So there's some gap */
                    match_list[i + 1] = match_list[j];
                i++;
                j++;
            }
            matches = i;
            match_list[matches + 1] = NULL;
            match_list[0] = g_strndup (match_list[1], low);
        }
    }
    else
    {                           /* There were no matches. */
        g_free (match_list);
        match_list = NULL;
    }
    return match_list;
}

/* --------------------------------------------------------------------------------------------- */
/** Check if directory completion is needed */
static gboolean
check_is_cd (const char *text, int lc_start, input_complete_t flags)
{
    char *p, *q;

    SHOW_C_CTX ("check_is_cd");

    if ((flags & INPUT_COMPLETE_CD) == 0)
        return FALSE;

    /* Skip initial spaces */
    p = (char *) text;
    q = (char *) text + lc_start;
    while (p < q && p[0] != '\0' && str_isspace (p))
        str_next_char (&p);

    /* Check if the command is "cd" and the cursor is after it */
    return (p[0] == 'c' && p[1] == 'd' && str_isspace (p + 2) && p + 2 < q);
}

/* --------------------------------------------------------------------------------------------- */
/** Returns an array of matches, or NULL if none. */
static char **
try_complete (char *text, int *lc_start, int *lc_end, input_complete_t flags)
{
    size_t in_command_position = 0;
    char *word;
    char **matches = NULL;
    char *p = NULL, *q = NULL, *r = NULL;
    gboolean is_cd;

    SHOW_C_CTX ("try_complete");
    word = g_strndup (text + *lc_start, *lc_end - *lc_start);

    is_cd = check_is_cd (text, *lc_start, flags);

    /* Determine if this could be a command word. It is if it appears at
       the start of the line (ignoring preceding whitespace), or if it
       appears after a character that separates commands. And we have to
       be in a INPUT_COMPLETE_COMMANDS flagged Input line. */
    if (!is_cd && (flags & INPUT_COMPLETE_COMMANDS))
    {
        const char *command_separator_chars = ";|&{(`";
        char *ti;

        if (*lc_start == 0)
            ti = text;
        else
        {
            ti = str_get_prev_char (&text[*lc_start]);
            while (ti > text && (ti[0] == ' ' || ti[0] == '\t'))
                str_prev_char (&ti);
        }

        if (ti == text)
            in_command_position++;
        else if (strchr (command_separator_chars, ti[0]) != NULL)
        {
            int this_char, prev_char;

            in_command_position++;

            if (ti != text)
            {
                /* Handle the two character tokens `>&', `<&', and `>|'.
                   We are not in a command position after one of these. */
                this_char = ti[0];
                prev_char = str_get_prev_char (ti)[0];

                /* Quoted */
                if ((this_char == '&' && (prev_char == '<' || prev_char == '>'))
                    || (this_char == '|' && prev_char == '>') || (ti != text
                                                                  && str_get_prev_char (ti)[0] ==
                                                                  '\\'))
                    in_command_position = 0;
            }
        }
    }

    if (flags & INPUT_COMPLETE_COMMANDS)
        p = strrchr (word, '`');
    if (flags & (INPUT_COMPLETE_COMMANDS | INPUT_COMPLETE_VARIABLES))
    {
        q = strrchr (word, '$');

        /* don't substitute variable in \$ case */
        if (q != NULL && q != word && q[-1] == '\\')
        {
            size_t qlen;

            qlen = strlen (q);
            /* drop '\\' */
            memmove (q - 1, q, qlen + 1);
            /* adjust flags */
            flags &= ~INPUT_COMPLETE_VARIABLES;
            q = NULL;
        }
    }
    if (flags & INPUT_COMPLETE_HOSTNAMES)
        r = strrchr (word, '@');
    if (q && q[1] == '(' && (flags & INPUT_COMPLETE_COMMANDS))
    {
        if (q > p)
            p = str_get_next_char (q);
        q = NULL;
    }

    /* Command substitution? */
    if (p > q && p > r)
    {
        SHOW_C_CTX ("try_complete:cmd_backq_subst");
        matches = completion_matches (str_cget_next_char (p),
                                      command_completion_function,
                                      flags & (~INPUT_COMPLETE_FILENAMES));
        if (matches)
            *lc_start += str_get_next_char (p) - word;
    }

    /* Variable name? */
    else if (q > p && q > r)
    {
        SHOW_C_CTX ("try_complete:var_subst");
        matches = completion_matches (q, variable_completion_function, flags);
        if (matches)
            *lc_start += q - word;
    }

    /* Starts with '@', then look through the known hostnames for 
       completion first. */
    else if (r > p && r > q)
    {
        SHOW_C_CTX ("try_complete:host_subst");
        matches = completion_matches (r, hostname_completion_function, flags);
        if (matches)
            *lc_start += r - word;
    }

    /* Starts with `~' and there is no slash in the word, then
       try completing this word as a username. */
    if (!matches && *word == '~' && (flags & INPUT_COMPLETE_USERNAMES) && !strchr (word, PATH_SEP))
    {
        SHOW_C_CTX ("try_complete:user_subst");
        matches = completion_matches (word, username_completion_function, flags);
    }


    /* And finally if this word is in a command position, then
       complete over possible command names, including aliases, functions,
       and command names. */
    if (!matches && in_command_position != 0)
    {
        SHOW_C_CTX ("try_complete:cmd_subst");
        matches =
            completion_matches (word, command_completion_function,
                                flags & (~INPUT_COMPLETE_FILENAMES));
    }

    else if (!matches && (flags & INPUT_COMPLETE_FILENAMES))
    {
        if (is_cd)
            flags &= ~(INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_COMMANDS);
        SHOW_C_CTX ("try_complete:filename_subst_1");
        matches = completion_matches (word, filename_completion_function, flags);
        if (!matches && is_cd && *word != PATH_SEP && *word != '~')
        {
            q = text + *lc_start;
            for (p = text; *p && p < q && (*p == ' ' || *p == '\t'); str_next_char (&p));
            if (!strncmp (p, "cd", 2))
                for (p += 2; *p && p < q && (*p == ' ' || *p == '\t'); str_next_char (&p));
            if (p == q)
            {
                char *const cdpath_ref = g_strdup (getenv ("CDPATH"));
                char *cdpath = cdpath_ref;
                char c, *s;

                if (cdpath == NULL)
                    c = 0;
                else
                    c = ':';
                while (!matches && c == ':')
                {
                    s = strchr (cdpath, ':');
                    if (s == NULL)
                        s = strchr (cdpath, 0);
                    c = *s;
                    *s = 0;
                    if (*cdpath)
                    {
                        r = mc_build_filename (cdpath, word, NULL);
                        SHOW_C_CTX ("try_complete:filename_subst_2");
                        matches = completion_matches (r, filename_completion_function, flags);
                        g_free (r);
                    }
                    *s = c;
                    cdpath = str_get_next_char (s);
                }
                g_free (cdpath_ref);
            }
        }
    }

    g_free (word);

    return matches;
}

/* --------------------------------------------------------------------------------------------- */

static int
insert_text (WInput * in, char *text, ssize_t size)
{
    int buff_len = str_length (in->buffer);

    size = min (size, (ssize_t) strlen (text)) + start - end;
    if (strlen (in->buffer) + size >= (size_t) in->current_max_size)
    {
        /* Expand the buffer */
        char *narea = g_try_realloc (in->buffer, in->current_max_size + size + in->field_width);
        if (narea != NULL)
        {
            in->buffer = narea;
            in->current_max_size += size + in->field_width;
        }
    }
    if (strlen (in->buffer) + 1 < (size_t) in->current_max_size)
    {
        if (size > 0)
        {
            int i = strlen (&in->buffer[end]);
            for (; i >= 0; i--)
                in->buffer[end + size + i] = in->buffer[end + i];
        }
        else if (size < 0)
        {
            char *p = in->buffer + end + size, *q = in->buffer + end;
            while (*q)
                *(p++) = *(q++);
            *p = 0;
        }
        memcpy (in->buffer + start, text, size - start + end);
        in->point += str_length (in->buffer) - buff_len;
        input_update (in, TRUE);
        end += size;
    }
    return size != 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
query_callback (WDialog * h, Widget * sender, dlg_msg_t msg, int parm, void *data)
{
    static char buff[MB_LEN_MAX] = "";
    static int bl = 0;

    switch (msg)
    {
    case DLG_KEY:
        switch (parm)
        {
        case KEY_LEFT:
        case KEY_RIGHT:
            bl = 0;
            h->ret_value = 0;
            dlg_stop (h);
            return MSG_HANDLED;

        case KEY_BACKSPACE:
            bl = 0;
            /* exit from completion list if input line is empty */
            if (end == 0)
            {
                h->ret_value = 0;
                dlg_stop (h);
            }
            /* Refill the list box and start again */
            else if (end == min_end)
            {
                end = str_get_prev_char (&input->buffer[end]) - input->buffer;
                input_handle_char (input, parm);
                h->ret_value = B_USER;
                dlg_stop (h);
                return MSG_HANDLED;
            }
            else
            {
                int new_end;
                int i;
                GList *e;

                new_end = str_get_prev_char (&input->buffer[end]) - input->buffer;

                for (i = 0, e = ((WListbox *) h->current->data)->list;
                     e != NULL; i++, e = g_list_next (e))
                {
                    WLEntry *le = (WLEntry *) e->data;

                    if (strncmp (input->buffer + start, le->text, new_end - start) == 0)
                    {
                        listbox_select_entry ((WListbox *) h->current->data, i);
                        end = new_end;
                        input_handle_char (input, parm);
                        send_message (WIDGET (h->current->data), NULL, WIDGET_DRAW, 0, NULL);
                        break;
                    }
                }
            }
            return MSG_HANDLED;

        default:
            if (parm < 32 || parm > 255)
            {
                bl = 0;
                if (input_key_is_in_map (input, parm) != 2)
                    return MSG_NOT_HANDLED;

                if (end == min_end)
                    return MSG_HANDLED;

                /* This means we want to refill the list box and start again */
                h->ret_value = B_USER;
                dlg_stop (h);
                return MSG_HANDLED;
            }
            else
            {
                GList *e;
                int i;
                int need_redraw = 0;
                int low = 4096;
                char *last_text = NULL;

                buff[bl++] = (char) parm;
                buff[bl] = '\0';
                switch (str_is_valid_char (buff, bl))
                {
                case -1:
                    bl = 0;
                    /* fallthrough */
                case -2:
                    return MSG_HANDLED;
                }

                for (i = 0, e = ((WListbox *) h->current->data)->list;
                     e != NULL; i++, e = g_list_next (e))
                {
                    WLEntry *le = (WLEntry *) e->data;

                    if (strncmp (input->buffer + start, le->text, end - start) == 0
                        && strncmp (&le->text[end - start], buff, bl) == 0)
                    {
                        if (need_redraw == 0)
                        {
                            need_redraw = 1;
                            listbox_select_entry ((WListbox *) h->current->data, i);
                            last_text = le->text;
                        }
                        else
                        {
                            char *si, *sl;
                            int si_num = 0;
                            int sl_num = 0;

                            /* count symbols between start and end */
                            for (si = le->text + start; si < le->text + end;
                                 str_next_char (&si), si_num++)
                                ;
                            for (sl = last_text + start; sl < last_text + end;
                                 str_next_char (&sl), sl_num++)
                                ;

                            /* pointers to next symbols */
                            si = &le->text[str_offset_to_pos (le->text, ++si_num)];
                            sl = &last_text[str_offset_to_pos (last_text, ++sl_num)];

                            while (si[0] != '\0' && sl[0] != '\0')
                            {
                                char *nexti, *nextl;

                                nexti = str_get_next_char (si);
                                nextl = str_get_next_char (sl);

                                if (nexti - si != nextl - sl || strncmp (si, sl, nexti - si) != 0)
                                    break;

                                si = nexti;
                                sl = nextl;

                                si_num++;
                            }

                            last_text = le->text;

                            si = &last_text[str_offset_to_pos (last_text, si_num)];
                            if (low > si - last_text)
                                low = si - last_text;

                            need_redraw = 2;
                        }
                    }
                }

                if (need_redraw == 2)
                {
                    insert_text (input, last_text, low);
                    send_message (WIDGET (h->current->data), NULL, WIDGET_DRAW, 0, NULL);
                }
                else if (need_redraw == 1)
                {
                    h->ret_value = B_ENTER;
                    dlg_stop (h);
                }
                bl = 0;
            }
            return MSG_HANDLED;
        }
        break;

    default:
        return dlg_default_callback (h, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** Returns 1 if the user would like to see us again */

static int
complete_engine (WInput * in, int what_to_do)
{
    if (in->completions != NULL && str_offset_to_pos (in->buffer, in->point) != end)
        input_free_completions (in);
    if (in->completions == NULL)
    {
        char *s;

        end = str_offset_to_pos (in->buffer, in->point);

        s = in->buffer;
        if (in->point != 0)
        {
            /* get symbol before in->point */
            size_t i;
            for (i = in->point - 1; i > 0; i--)
                str_next_char (&s);
        }

        for (; s >= in->buffer; str_prev_char (&s))
        {
            start = s - in->buffer;
            if (strchr (" \t;|<>", *s) != NULL)
                break;
        }

        if (start < end)
        {
            str_next_char (&s);
            start = s - in->buffer;
        }

        in->completions = try_complete (in->buffer, &start, &end, in->completion_flags);
    }

    if (in->completions != NULL)
    {
        if (what_to_do & DO_INSERTION || ((what_to_do & DO_QUERY) && !in->completions[1]))
        {
            char *lc_complete = in->completions[0];
            if (insert_text (in, lc_complete, strlen (lc_complete)))
            {
                if (in->completions[1])
                    tty_beep ();
                else
                    input_free_completions (in);
            }
            else
                tty_beep ();
        }
        if ((what_to_do & DO_QUERY) && in->completions && in->completions[1])
        {
            int maxlen = 0, i, count = 0;
            int x, y, w, h;
            int start_x, start_y;
            char **p, *q;
            WDialog *query_dlg;
            WListbox *query_list;

            for (p = in->completions + 1; *p != NULL; count++, p++)
            {
                i = str_term_width1 (*p);
                if (i > maxlen)
                    maxlen = i;
            }
            start_x = WIDGET (in)->x;
            start_y = WIDGET (in)->y;
            if (start_y - 2 >= count)
            {
                y = start_y - 2 - count;
                h = 2 + count;
            }
            else
            {
                if (start_y >= LINES - start_y - 1)
                {
                    y = 0;
                    h = start_y;
                }
                else
                {
                    y = start_y + 1;
                    h = LINES - start_y - 1;
                }
            }
            x = start - in->term_first_shown - 2 + start_x;
            w = maxlen + 4;
            if (x + w > COLS)
                x = COLS - w;
            if (x < 0)
                x = 0;
            if (x + w > COLS)
                w = COLS;
            input = in;
            min_end = end;
            query_height = h;
            query_width = w;
            query_dlg = create_dlg (TRUE, y, x, query_height, query_width,
                                    dialog_colors, query_callback, NULL,
                                    "[Completion]", NULL, DLG_COMPACT);
            query_list = listbox_new (1, 1, h - 2, w - 2, FALSE, NULL);
            add_widget (query_dlg, query_list);
            for (p = in->completions + 1; *p; p++)
                listbox_add_item (query_list, LISTBOX_APPEND_AT_END, 0, *p, NULL);
            run_dlg (query_dlg);
            q = NULL;
            if (query_dlg->ret_value == B_ENTER)
            {
                listbox_get_current (query_list, &q, NULL);
                if (q)
                    insert_text (in, q, strlen (q));
            }
            if (q || end != min_end)
                input_free_completions (in);
            i = query_dlg->ret_value;   /* B_USER if user wants to start over again */
            destroy_dlg (query_dlg);
            if (i == B_USER)
                return 1;
        }
    }
    else
        tty_beep ();
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* declared in lib/widget/input.h */
void
complete (WInput * in)
{
    int engine_flags;

    if (!str_is_valid_string (in->buffer))
        return;

    if (in->completions != NULL)
        engine_flags = DO_QUERY;
    else
    {
        engine_flags = DO_INSERTION;

        if (mc_global.widget.show_all_if_ambiguous)
            engine_flags |= DO_QUERY;
    }

    while (complete_engine (in, engine_flags))
        ;
}

/* --------------------------------------------------------------------------------------------- */
