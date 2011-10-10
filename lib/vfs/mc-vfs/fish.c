/* Virtual File System: FISH implementation for transfering files over
   shell connections.

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
   2007 Free Software Foundation, Inc.

   Written by: 1998 Pavel Machek
   Spaces fix: 2000 Michal Svec
   2010 Andrew Borodin
   2010 Slava Zanko
   2010 Ilia Maslakov

   Derived from ftpfs.c.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/**
 * \file
 * \brief Source: Virtual File System: FISH implementation for transfering files over
 * shell connections
 * \author Pavel Machek
 * \author Michal Svec
 * \date 1998, 2000
 *
 * Derived from ftpfs.c
 * Read README.fish for protocol specification.
 *
 * Syntax of path is: \verbatim /#sh:user@host[:Cr]/path \endverbatim
 *      where C means you want compressed connection,
 *      and r means you want to use rsh
 *
 * Namespace: fish_vfs_ops exported.
 */

/* Define this if your ssh can take -I option */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>           /* gettimeofday() */
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/tty/tty.h"        /* enable/disable interrupt key */
#include "lib/strescape.h"
#include "lib/unixcompat.h"
#include "lib/fileloc.h"

#include "src/filemanager/layout.h"     /* print_vfs_message */
#include "src/execute.h"        /* pre_exec, post_exec */

#include "vfs-impl.h"
#include "utilvfs.h"
#include "netutil.h"
#include "xdirentry.h"
#include "gc.h"                 /* vfs_stamp_create */

#include "fish.h"
#include "fishdef.h"

/*** global variables ****************************************************************************/

int fish_directory_timeout = 900;

/*** file scope macro definitions ****************************************************************/

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

#define FISH_FLAG_COMPRESSED 1
#define FISH_FLAG_RSH        2

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

/*
 * Reply codes.
 */
#define PRELIM          1       /* positive preliminary */
#define COMPLETE        2       /* positive completion */
#define CONTINUE        3       /* positive intermediate */
#define TRANSIENT       4       /* transient negative completion */
#define ERROR           5       /* permanent negative completion */

/* command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02

/* environment flags */
#define FISH_HAVE_HEAD         1
#define FISH_HAVE_SED          2
#define FISH_HAVE_AWK          4
#define FISH_HAVE_PERL         8
#define FISH_HAVE_LSQ         16
#define FISH_HAVE_DATE_MDYT   32
#define FISH_HAVE_TAIL        64

#define SUP super->u.fish

#define PREFIX \
    char buf[BUF_LARGE]; \
    const char *crpath; \
    char *rpath, *mpath; \
    struct vfs_s_super *super; \
    mpath = g_strdup (path); \
    crpath = vfs_s_get_path_mangle (me, mpath, &super, 0); \
    if (crpath == NULL) \
    { \
       g_free (mpath); \
       return -1; \
    } \
    rpath = strutils_shell_escape (crpath); \
    g_free (mpath)

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static char reply_str[80];

static struct vfs_class vfs_fish_ops;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static char *
fish_load_script_from_file (const char *hostname, const char *script_name, const char *def_content)
{
    char *scr_filename = NULL;
    char *scr_content;
    gsize scr_len = 0;

    /* 1st: scan user directory */
    scr_filename = g_build_path (PATH_SEP_STR, home_dir, MC_USERCONF_DIR, FISH_PREFIX, hostname,
                                 script_name, (char *) NULL);
    /* silent about user dir */
    g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
    g_free (scr_filename);
    /* 2nd: scan system dir */
    if (scr_content == NULL)
    {
        scr_filename =
            g_build_path (PATH_SEP_STR, LIBEXECDIR, FISH_PREFIX, script_name, (char *) NULL);
        g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
        g_free (scr_filename);
    }

    if (scr_content != NULL)
        return scr_content;

    return g_strdup (def_content);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_decode_reply (char *s, int was_garbage)
{
    int code;
    if (!sscanf (s, "%d", &code))
    {
        code = 500;
        return 5;
    }
    if (code < 100)
        return was_garbage ? ERROR : (!code ? COMPLETE : PRELIM);
    return code / 100;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */

static int
fish_get_reply (struct vfs_class *me, int sock, char *string_buf, int string_len)
{
    char answer[1024];
    int was_garbage = 0;

    for (;;)
    {
        if (!vfs_s_get_line (me, sock, answer, sizeof (answer), '\n'))
        {
            if (string_buf)
                *string_buf = 0;
            return 4;
        }

        if (strncmp (answer, "### ", 4))
        {
            was_garbage = 1;
            if (string_buf)
                g_strlcpy (string_buf, answer, string_len);
        }
        else
            return fish_decode_reply (answer + 4, was_garbage);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_command (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *fmt, ...)
{
    va_list ap;
    char *str;
    int status;
    FILE *logfile = MEDATA->logfile;

    va_start (ap, fmt);

    str = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    if (logfile)
    {
        size_t ret;
        ret = fwrite (str, strlen (str), 1, logfile);
        ret = fflush (logfile);
    }

    tty_enable_interrupt_key ();

    status = write (SUP.sockw, str, strlen (str));
    g_free (str);

    tty_disable_interrupt_key ();
    if (status < 0)
        return TRANSIENT;

    if (wait_reply)
        return fish_get_reply (me, SUP.sockr,
                               (wait_reply & WANT_STRING) ? reply_str :
                               NULL, sizeof (reply_str) - 1);
    return COMPLETE;
}

/* --------------------------------------------------------------------------------------------- */

static void
fish_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    if ((SUP.sockw != -1) || (SUP.sockr != -1))
    {
        print_vfs_message (_("fish: Disconnecting from %s"), super->name ? super->name : "???");
        fish_command (me, super, NONE, "#BYE\nexit\n");
        close (SUP.sockw);
        close (SUP.sockr);
        SUP.sockw = SUP.sockr = -1;
    }
    g_free (SUP.host);
    g_free (SUP.user);
    g_free (SUP.cwdir);
    g_free (SUP.password);
    g_free (SUP.scr_ls);
    g_free (SUP.scr_exists);
    g_free (SUP.scr_mkdir);
    g_free (SUP.scr_unlink);
    g_free (SUP.scr_chown);
    g_free (SUP.scr_chmod);
    g_free (SUP.scr_utime);
    g_free (SUP.scr_rmdir);
    g_free (SUP.scr_ln);
    g_free (SUP.scr_mv);
    g_free (SUP.scr_hardlink);
    g_free (SUP.scr_get);
    g_free (SUP.scr_send);
    g_free (SUP.scr_append);
    g_free (SUP.scr_info);
    g_free (SUP.scr_env);
}

/* --------------------------------------------------------------------------------------------- */

static void
fish_pipeopen (struct vfs_s_super *super, const char *path, const char *argv[])
{
    int fileset1[2], fileset2[2];
    int res;

    if ((pipe (fileset1) < 0) || (pipe (fileset2) < 0))
        vfs_die ("Cannot pipe(): %m.");

    res = fork ();

    if (res != 0)
    {
        if (res < 0)
            vfs_die ("Cannot fork(): %m.");
        /* We are the parent */
        close (fileset1[0]);
        SUP.sockw = fileset1[1];
        close (fileset2[1]);
        SUP.sockr = fileset2[0];
    }
    else
    {
        res = dup2 (fileset1[0], 0);
        close (fileset1[0]);
        close (fileset1[1]);
        res = dup2 (fileset2[1], 1);
        close (2);
        /* stderr to /dev/null */
        res = open ("/dev/null", O_WRONLY);
        close (fileset2[0]);
        close (fileset2[1]);
        execvp (path, const_cast (char **, argv));
        _exit (3);
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
fish_set_env (int flags)
{
    GString *tmp;

    tmp = g_string_sized_new (250);
    g_string_assign (tmp, "");

    if ((flags & FISH_HAVE_HEAD) != 0)
        g_string_append (tmp, "FISH_HAVE_HEAD=1 export FISH_HAVE_HEAD; ");

    if ((flags & FISH_HAVE_SED) != 0)
        g_string_append (tmp, "FISH_HAVE_SED=1 export FISH_HAVE_SED; ");

    if ((flags & FISH_HAVE_AWK) != 0)
        g_string_append (tmp, "FISH_HAVE_AWK=1 export FISH_HAVE_AWK; ");

    if ((flags & FISH_HAVE_PERL) != 0)
        g_string_append (tmp, "FISH_HAVE_PERL=1 export FISH_HAVE_PERL; ");

    if ((flags & FISH_HAVE_LSQ) != 0)
        g_string_append (tmp, "FISH_HAVE_LSQ=1 export FISH_HAVE_LSQ; ");

    if ((flags & FISH_HAVE_DATE_MDYT) != 0)
        g_string_append (tmp, "FISH_HAVE_DATE_MDYT=1 export FISH_HAVE_DATE_MDYT; ");

    if ((flags & FISH_HAVE_TAIL) != 0)
        g_string_append (tmp, "FISH_HAVE_TAIL=1 export FISH_HAVE_TAIL; ");

    return g_string_free (tmp, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
fish_info (struct vfs_class *me, struct vfs_s_super *super)
{
    char buffer[8192];
    if (fish_command (me, super, NONE, SUP.scr_info) == COMPLETE)
    {
        while (1)
        {
            int res;
            res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), SUP.sockr);
            if ((!res) || (res == EINTR))
                ERRNOR (ECONNRESET, FALSE);
            if (!strncmp (buffer, "### ", 4))
                break;
            SUP.host_flags = atol (buffer);
        }
        return TRUE;
    }
    ERRNOR (E_PROTO, FALSE);
}


/* --------------------------------------------------------------------------------------------- */
/* The returned directory should always contain a trailing slash */

static char *
fish_getcwd (struct vfs_class *me, struct vfs_s_super *super)
{
    if (fish_command (me, super, WANT_STRING, "#PWD\npwd; echo '### 200'\n") == COMPLETE)
        return g_strconcat (reply_str, "/", (char *) NULL);
    ERRNOR (EIO, NULL);
}


/* --------------------------------------------------------------------------------------------- */

static void
fish_open_archive_pipeopen (struct vfs_s_super *super)
{
    char gbuf[10];
    const char *argv[10];       /* All of 10 is used now */
    const char *xsh = (SUP.flags == FISH_FLAG_RSH ? "rsh" : "ssh");
    int i = 0;

    argv[i++] = xsh;
    if (SUP.flags == FISH_FLAG_COMPRESSED)
        argv[i++] = "-C";

    if (SUP.flags > FISH_FLAG_RSH)
    {
        argv[i++] = "-p";
        g_snprintf (gbuf, sizeof (gbuf), "%d", SUP.flags);
        argv[i++] = gbuf;
    }

    /*
     * Add the user name to the ssh command line only if it was explicitly
     * set in vfs URL. rsh/ssh will get current user by default
     * plus we can set convenient overrides in  ~/.ssh/config (explicit -l
     * option breaks it for some)
     */

    if (SUP.user)
    {
        argv[i++] = "-l";
        argv[i++] = SUP.user;
    }
    else
    {
        /* The rest of the code assumes it to be a valid username */
        SUP.user = vfs_get_local_username ();
    }

    argv[i++] = SUP.host;
    argv[i++] = "echo FISH:; /bin/sh";
    argv[i++] = NULL;

    fish_pipeopen (super, xsh, argv);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
fish_open_archive_talk (struct vfs_class *me, struct vfs_s_super *super)
{
    char answer[2048];

    printf ("\n%s\n", _("fish: Waiting for initial line..."));

    if (!vfs_s_get_line (me, SUP.sockr, answer, sizeof (answer), ':'))
        return FALSE;

    if (strstr (answer, "assword"))
    {
        /* Currently, this does not work. ssh reads passwords from
           /dev/tty, not from stdin :-(. */

        printf ("\n%s\n", _("Sorry, we cannot do password authenticated connections for now."));

        return FALSE;
#if 0
        if (!SUP.password)
        {
            char *p, *op;
            p = g_strdup_printf (_("fish: Password is required for %s"), SUP.user);
            op = vfs_get_password (p);
            g_free (p);
            if (op == NULL)
                return FALSE;
            SUP.password = op;
        }
        printf ("\n%s\n", _("fish: Sending password..."));
        {
            size_t str_len;
            str_len = strlen (SUP.password);
            if ((write (SUP.sockw, SUP.password, str_len) != (ssize_t) str_len)
                || (write (SUP.sockw, "\n", 1) != 1))
            {
                return FALSE;
            }
        }
#endif
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_open_archive_int (struct vfs_class *me, struct vfs_s_super *super)
{
    gboolean ftalk;
    /* hide panels */
    pre_exec ();

    /* open pipe */
    fish_open_archive_pipeopen (super);

    /* Start talk with ssh-server (password prompt, etc ) */
    ftalk = fish_open_archive_talk (me, super);

    /* show panels */
    post_exec ();

    if (!ftalk)
        ERRNOR (E_PROTO, -1);

    print_vfs_message (_("fish: Sending initial line..."));
    /*
     * Run `start_fish_server'. If it doesn't exist - no problem,
     * we'll talk directly to the shell.
     */

    if (fish_command
        (me, super, WAIT_REPLY,
         "#FISH\necho; start_fish_server 2>&1; echo '### 200'\n") != COMPLETE)
        ERRNOR (E_PROTO, -1);

    print_vfs_message (_("fish: Handshaking version..."));
    if (fish_command (me, super, WAIT_REPLY, "#VER 0.0.3\necho '### 000'\n") != COMPLETE)
        ERRNOR (E_PROTO, -1);

    /* Set up remote locale to C, otherwise dates cannot be recognized */
    if (fish_command
        (me, super, WAIT_REPLY,
         "LANG=C LC_ALL=C LC_TIME=C; export LANG LC_ALL LC_TIME;\n" "echo '### 200'\n") != COMPLETE)
        ERRNOR (E_PROTO, -1);

    print_vfs_message (_("fish: Getting host info..."));
    if (fish_info (me, super))
        SUP.scr_env = fish_set_env (SUP.host_flags);

    print_vfs_message (_("fish: Setting up current directory..."));
    SUP.cwdir = fish_getcwd (me, super);
    print_vfs_message (_("fish: Connected, home %s."), SUP.cwdir);
#if 0
    super->name = g_strconcat ("/#sh:", SUP.user, "@", SUP.host, "/", (char *) NULL);
#endif
    super->name = g_strdup (PATH_SEP_STR);

    super->root = vfs_s_new_inode (me, super, vfs_s_default_stat (me, S_IFDIR | 0755));
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_open_archive (struct vfs_class *me, struct vfs_s_super *super,
                   const char *archive_name, char *op)
{
    char *host, *user, *password, *p;
    int flags;

    (void) archive_name;

    p = vfs_split_url (strchr (op, ':') + 1, &host, &user, &flags,
                       &password, 0, URL_NOSLASH | URL_USE_ANONYMOUS);

    g_free (p);

    SUP.host = host;
    SUP.user = user;
    SUP.flags = flags;
    if (!strncmp (op, "rsh:", 4))
        SUP.flags = FISH_FLAG_RSH;
    SUP.cwdir = NULL;
    if (password)
        SUP.password = password;
    SUP.scr_ls = fish_load_script_from_file (host, FISH_LS_FILE, FISH_LS_DEF_CONTENT);
    SUP.scr_exists = fish_load_script_from_file (host, FISH_EXISTS_FILE, FISH_EXISTS_DEF_CONTENT);
    SUP.scr_mkdir = fish_load_script_from_file (host, FISH_MKDIR_FILE, FISH_MKDIR_DEF_CONTENT);
    SUP.scr_unlink = fish_load_script_from_file (host, FISH_UNLINK_FILE, FISH_UNLINK_DEF_CONTENT);
    SUP.scr_chown = fish_load_script_from_file (host, FISH_CHOWN_FILE, FISH_CHOWN_DEF_CONTENT);
    SUP.scr_chmod = fish_load_script_from_file (host, FISH_CHMOD_FILE, FISH_CHMOD_DEF_CONTENT);
    SUP.scr_utime = fish_load_script_from_file (host, FISH_UTIME_FILE, FISH_UTIME_DEF_CONTENT);
    SUP.scr_rmdir = fish_load_script_from_file (host, FISH_RMDIR_FILE, FISH_RMDIR_DEF_CONTENT);
    SUP.scr_ln = fish_load_script_from_file (host, FISH_LN_FILE, FISH_LN_DEF_CONTENT);
    SUP.scr_mv = fish_load_script_from_file (host, FISH_MV_FILE, FISH_MV_DEF_CONTENT);
    SUP.scr_hardlink =
        fish_load_script_from_file (host, FISH_HARDLINK_FILE, FISH_HARDLINK_DEF_CONTENT);
    SUP.scr_get = fish_load_script_from_file (host, FISH_GET_FILE, FISH_GET_DEF_CONTENT);
    SUP.scr_send = fish_load_script_from_file (host, FISH_SEND_FILE, FISH_SEND_DEF_CONTENT);
    SUP.scr_append = fish_load_script_from_file (host, FISH_APPEND_FILE, FISH_APPEND_DEF_CONTENT);
    SUP.scr_info = fish_load_script_from_file (host, FISH_INFO_FILE, FISH_INFO_DEF_CONTENT);
    return fish_open_archive_int (me, super);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_archive_same (struct vfs_class *me, struct vfs_s_super *super,
                   const char *archive_name, char *op, void *cookie)
{
    char *host, *user;
    int flags;
    int result;

    (void) me;
    (void) archive_name;
    (void) cookie;

    op = vfs_split_url (strchr (op, ':') + 1, &host, &user, &flags, 0, 0,
                        URL_NOSLASH | URL_USE_ANONYMOUS);

    g_free (op);

    if (user == NULL)
        user = vfs_get_local_username ();

    result = ((strcmp (host, SUP.host) == 0)
              && (strcmp (user, SUP.user) == 0) && (flags == SUP.flags));

    g_free (host);
    g_free (user);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
{
    struct vfs_s_super *super = dir->super;
    char buffer[8192];
    struct vfs_s_entry *ent = NULL;
    FILE *logfile;
    char *quoted_path;
    int reply_code;
    gchar *shell_commands;

    /*
     * Simple FISH debug interface :]
     */
#if 0
    if (!(MEDATA->logfile))
    {
        MEDATA->logfile = fopen ("/tmp/mc-FISH.sh", "w");
    }
#endif
    logfile = MEDATA->logfile;

    print_vfs_message (_("fish: Reading directory %s..."), remote_path);

    gettimeofday (&dir->timestamp, NULL);
    dir->timestamp.tv_sec += fish_directory_timeout;
    quoted_path = strutils_shell_escape (remote_path);
    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s;\n", SUP.scr_ls, (char *) NULL);
    fish_command (me, super, NONE, shell_commands, quoted_path);
    g_free (shell_commands);
    g_free (quoted_path);
    ent = vfs_s_generate_entry (me, NULL, dir, 0);
    while (1)
    {
        int res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), SUP.sockr);
        if ((!res) || (res == EINTR))
        {
            vfs_s_free_entry (me, ent);
            me->verrno = ECONNRESET;
            goto error;
        }
        if (logfile)
        {
            fputs (buffer, logfile);
            fputs ("\n", logfile);
            fflush (logfile);
        }
        if (!strncmp (buffer, "### ", 4))
            break;
        if ((!buffer[0]))
        {
            if (ent->name)
            {
                vfs_s_insert_entry (me, dir, ent);
                ent = vfs_s_generate_entry (me, NULL, dir, 0);
            }
            continue;
        }

#define ST ent->ino->st

        switch (buffer[0])
        {
        case ':':
            {
                char *temp;
                char *data_start = buffer + 1;
                char *filename = data_start;
                char *linkname = data_start;
                char *filename_bound = filename + strlen (filename);
                char *linkname_bound = filename_bound;
                if (!strcmp (data_start, "\".\"") || !strcmp (data_start, "\"..\""))
                    break;      /* We'll do "." and ".." ourselves */

                if (S_ISLNK (ST.st_mode))
                {
                    /* we expect: "escaped-name" -> "escaped-name"
                       //     -> cannot occur in filenames,
                       //     because it will be escaped to -\> */

                    if (*filename == '"')
                        ++filename;

                    linkname = strstr (filename, "\" -> \"");
                    if (!linkname)
                    {
                        /* broken client, or smth goes wrong */
                        linkname = filename_bound;
                        if (filename_bound > filename && *(filename_bound - 1) == '"')
                            --filename_bound;   /* skip trailing " */
                    }
                    else
                    {
                        filename_bound = linkname;
                        linkname += 6;  /* strlen ("\" -> \"") */
                        if (*(linkname_bound - 1) == '"')
                            --linkname_bound;   /* skip trailing " */
                    }

                    ent->name = g_strndup (filename, filename_bound - filename);
                    temp = ent->name;
                    ent->name = strutils_shell_unescape (ent->name);
                    g_free (temp);

                    ent->ino->linkname = g_strndup (linkname, linkname_bound - linkname);
                    temp = ent->ino->linkname;
                    ent->ino->linkname = strutils_shell_unescape (ent->ino->linkname);
                    g_free (temp);
                }
                else
                {
                    /* we expect: "escaped-name" */
                    if (filename_bound - filename > 2)
                    {
                        /*
                           there is at least 2 "
                           and we skip them
                         */
                        if (*filename == '"')
                            ++filename;
                        if (*(filename_bound - 1) == '"')
                            --filename_bound;
                    }
                    ent->name = g_strndup (filename, filename_bound - filename);
                    temp = ent->name;
                    ent->name = strutils_shell_unescape (ent->name);
                    g_free (temp);
                }
                break;
            }
        case 'S':
#ifdef HAVE_ATOLL
            ST.st_size = (off_t) atoll (buffer + 1);
#else
            ST.st_size = (off_t) atof (buffer + 1);
#endif
            break;
        case 'P':
            {
                size_t skipped;
                vfs_parse_filemode (buffer + 1, &skipped, &ST.st_mode);
                break;
            }
        case 'R':
            {
                /*
                   raw filemode:
                   we expect: Roctal-filemode octal-filetype uid.gid
                 */
                size_t skipped;
                vfs_parse_raw_filemode (buffer + 1, &skipped, &ST.st_mode);
                break;
            }
        case 'd':
            {
                vfs_split_text (buffer + 1);
                if (!vfs_parse_filedate (0, &ST.st_ctime))
                    break;
                ST.st_atime = ST.st_mtime = ST.st_ctime;
            }
            break;
        case 'D':
            {
                struct tm tim;
                if (sscanf (buffer + 1, "%d %d %d %d %d %d", &tim.tm_year, &tim.tm_mon,
                            &tim.tm_mday, &tim.tm_hour, &tim.tm_min, &tim.tm_sec) != 6)
                    break;
                ST.st_atime = ST.st_mtime = ST.st_ctime = mktime (&tim);
            }
            break;
        case 'E':
            {
                int maj, min;
                if (sscanf (buffer + 1, "%d,%d", &maj, &min) != 2)
                    break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
                ST.st_rdev = makedev (maj, min);
#endif
            }
        }
    }

    vfs_s_free_entry (me, ent);
    reply_code = fish_decode_reply (buffer + 4, 0);
    if (reply_code == COMPLETE)
    {
        g_free (SUP.cwdir);
        SUP.cwdir = g_strdup (remote_path);
        print_vfs_message (_("%s: done."), me->name);
        return 0;
    }
    else if (reply_code == ERROR)
    {
        me->verrno = EACCES;
    }
    else
    {
        me->verrno = E_REMOTE;
    }

  error:
    print_vfs_message (_("%s: failure"), me->name);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_file_store (struct vfs_class *me, struct vfs_s_fh *fh, char *name, char *localname)
{
    gchar *shell_commands = NULL;
    struct vfs_s_super *super = FH_SUPER;
    int n, total;
    char buffer[8192];
    struct stat s;
    int was_error = 0;
    int h;
    char *quoted_name;

    h = open (localname, O_RDONLY);

    if (h == -1)
        ERRNOR (EIO, -1);
    if (fstat (h, &s) < 0)
    {
        close (h);
        ERRNOR (EIO, -1);
    }

    /* First, try this as stor:
     *
     *     ( head -c number ) | ( cat > file; cat >/dev/null )
     *
     *  If `head' is not present on the remote system, `dd' will be used.
     * Unfortunately, we cannot trust most non-GNU `head' implementations
     * even if `-c' options is supported. Therefore, we separate GNU head
     * (and other modern heads?) using `-q' and `-' . This causes another
     * implementations to fail (because of "incorrect options").
     *
     *  Fallback is:
     *
     *     rest=<number>
     *     while [ $rest -gt 0 ]
     *     do
     *        cnt=`expr \( $rest + 255 \) / 256`
     *        n=`dd bs=256 count=$cnt | tee -a <target_file> | wc -c`
     *        rest=`expr $rest - $n`
     *     done
     *
     *  `dd' was not designed for full filling of input buffers,
     *  and does not report exact number of bytes (not blocks).
     *  Therefore a more complex shell script is needed.
     *
     *   On some systems non-GNU head writes "Usage:" error report to stdout
     *  instead of stderr. It makes impossible the use of "head || dd"
     *  algorithm for file appending case, therefore just "dd" is used for it.
     */

    quoted_name = strutils_shell_escape (name);
    print_vfs_message (_("fish: store %s: sending command..."), quoted_name);

    /* FIXME: File size is limited to ULONG_MAX */
    if (!fh->u.fish.append)
    {
        shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s FISH_FILESIZE=%" PRIuMAX ";\n",
                                      SUP.scr_append, (char *) NULL);
        n = fish_command (me, super, WAIT_REPLY, shell_commands, quoted_name,
                          (uintmax_t) s.st_size);
        g_free (shell_commands);
    }
    else
    {
        shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s FISH_FILESIZE=%" PRIuMAX ";\n",
                                      SUP.scr_send, (char *) NULL);
        n = fish_command (me, super, WAIT_REPLY, shell_commands, quoted_name,
                          (uintmax_t) s.st_size);
        g_free (shell_commands);
    }
    if (n != PRELIM)
    {
        close (h);
        ERRNOR (E_REMOTE, -1);
    }

    total = 0;

    while (1)
    {
        int t;
        while ((n = read (h, buffer, sizeof (buffer))) < 0)
        {
            if ((errno == EINTR) && tty_got_interrupt ())
                continue;
            print_vfs_message (_("fish: Local read failed, sending zeros"));
            close (h);
            h = open ("/dev/zero", O_RDONLY);
        }

        if (n == 0)
            break;

        t = write (SUP.sockw, buffer, n);
        if (t != n)
        {
            if (t == -1)
                me->verrno = errno;
            else
                me->verrno = EIO;
            goto error_return;
        }
        tty_disable_interrupt_key ();
        total += n;
        print_vfs_message ("%s: %d/%" PRIuMAX,
                           was_error ? _("fish: storing zeros") : _("fish: storing file"),
                           total, (uintmax_t) s.st_size);
    }
    close (h);
    g_free (quoted_name);
    if ((fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE) || was_error)
        ERRNOR (E_REMOTE, -1);
    return 0;
  error_return:
    close (h);
    fish_get_reply (me, SUP.sockr, NULL, 0);
    g_free (quoted_name);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_linear_start (struct vfs_class *me, struct vfs_s_fh *fh, off_t offset)
{
    gchar *shell_commands = NULL;
    struct vfs_s_super *super = FH_SUPER;
    char *name;
    char *quoted_name;

    name = vfs_s_fullpath (me, fh->ino);
    if (name == NULL)
        return 0;
    quoted_name = strutils_shell_escape (name);
    g_free (name);
    fh->u.fish.append = 0;

    /*
     * Check whether the remote file is readable by using `dd' to copy 
     * a single byte from the remote file to /dev/null. If `dd' completes
     * with exit status of 0 use `cat' to send the file contents to the
     * standard output (i.e. over the network).
     */

    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s FISH_START_OFFSET=%" PRIuMAX ";\n",
                                  SUP.scr_get, (char *) NULL);
    offset = fish_command (me, super, WANT_STRING, shell_commands, quoted_name, (uintmax_t) offset);
    g_free (shell_commands);
    g_free (quoted_name);
    if (offset != PRELIM)
        ERRNOR (E_REMOTE, 0);
    fh->linear = LS_LINEAR_OPEN;
    fh->u.fish.got = 0;
    errno = 0;
#if SIZEOF_OFF_T == SIZEOF_LONG
    fh->u.fish.total = (off_t) strtol (reply_str, NULL, 10);
#else
    fh->u.fish.total = (off_t) strtoll (reply_str, NULL, 10);
#endif
    if (errno != 0)
        ERRNOR (E_REMOTE, 0);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
fish_linear_abort (struct vfs_class *me, struct vfs_s_fh *fh)
{
    struct vfs_s_super *super = FH_SUPER;
    char buffer[8192];
    int n;

    print_vfs_message (_("Aborting transfer..."));
    do
    {
        n = MIN (8192, fh->u.fish.total - fh->u.fish.got);
        if (n != 0)
        {
            n = read (SUP.sockr, buffer, n);
            if (n < 0)
                return;
            fh->u.fish.got += n;
        }
    }
    while (n != 0);

    if (fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)
        print_vfs_message (_("Error reported after abort."));
    else
        print_vfs_message (_("Aborted transfer would be successful."));
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_linear_read (struct vfs_class *me, struct vfs_s_fh *fh, void *buf, size_t len)
{
    struct vfs_s_super *super = FH_SUPER;
    ssize_t n = 0;
    len = MIN ((size_t) (fh->u.fish.total - fh->u.fish.got), len);
    tty_disable_interrupt_key ();
    while (len != 0 && ((n = read (SUP.sockr, buf, len)) < 0))
    {
        if ((errno == EINTR) && !tty_got_interrupt ())
            continue;
        break;
    }
    tty_enable_interrupt_key ();

    if (n > 0)
        fh->u.fish.got += n;
    else if (n < 0)
        fish_linear_abort (me, fh);
    else if (fish_get_reply (me, SUP.sockr, NULL, 0) != COMPLETE)
        ERRNOR (E_REMOTE, -1);
    ERRNOR (errno, n);
}

/* --------------------------------------------------------------------------------------------- */

static void
fish_linear_close (struct vfs_class *me, struct vfs_s_fh *fh)
{
    if (fh->u.fish.total != fh->u.fish.got)
        fish_linear_abort (me, fh);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_ctl (void *fh, int ctlop, void *arg)
{
    (void) arg;
    (void) fh;
    (void) ctlop;
    return 0;
#if 0
    switch (ctlop)
    {
    case VFS_CTL_IS_NOTREADY:
        {
            int v;

            if (!FH->linear)
                vfs_die ("You may not do this");
            if (FH->linear == LS_LINEAR_CLOSED || FH->linear == LS_LINEAR_PREOPEN)
                return 0;

            v = vfs_s_select_on_two (FH_SUPER->u.fish.sockr, 0);
            if (((v < 0) && (errno == EINTR)) || v == 0)
                return 1;
            return 0;
        }
    default:
        return 0;
    }
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_send_command (struct vfs_class *me, struct vfs_s_super *super, const char *cmd, int flags)
{
    int r;

    r = fish_command (me, super, WAIT_REPLY, "%s", cmd);
    vfs_stamp_create (&vfs_fish_ops, super);
    if (r != COMPLETE)
        ERRNOR (E_REMOTE, -1);
    if (flags & OPT_FLUSH)
        vfs_s_invalidate (me, super);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_rename (struct vfs_class *me, const char *path1, const char *path2)
{
    gchar *shell_commands = NULL;
    char buf[BUF_LARGE];
    const char *crpath1, *crpath2;
    char *rpath1, *rpath2, *mpath1, *mpath2;
    struct vfs_s_super *super, *super2;

    mpath1 = g_strdup (path1);
    crpath1 = vfs_s_get_path_mangle (me, mpath1, &super, 0);
    if (crpath1 == NULL)
    {
        g_free (mpath1);
        return -1;
    }
    mpath2 = g_strdup (path2);
    crpath2 = vfs_s_get_path_mangle (me, mpath2, &super2, 0);
    if (crpath2 == NULL)
    {
        g_free (mpath1);
        g_free (mpath2);
        return -1;
    }
    rpath1 = strutils_shell_escape (crpath1);
    g_free (mpath1);
    rpath2 = strutils_shell_escape (crpath2);
    g_free (mpath2);
    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILEFROM=%s FISH_FILETO=%s;\n",
                                  SUP.scr_mv, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath1, rpath2);
    g_free (shell_commands);
    g_free (rpath1);
    g_free (rpath2);
    return fish_send_command (me, super2, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_link (struct vfs_class *me, const char *path1, const char *path2)
{
    gchar *shell_commands = NULL;
    char buf[BUF_LARGE];
    const char *crpath1, *crpath2;
    char *rpath1, *rpath2, *mpath1, *mpath2;
    struct vfs_s_super *super, *super2;

    mpath1 = g_strdup (path1);
    crpath1 = vfs_s_get_path_mangle (me, mpath1, &super, 0);
    if (crpath1 == NULL)
    {
        g_free (mpath1);
        return -1;
    }
    mpath2 = g_strdup (path2);
    crpath2 = vfs_s_get_path_mangle (me, mpath2, &super2, 0);
    if (crpath2 == NULL)
    {
        g_free (mpath1);
        g_free (mpath2);
        return -1;
    }
    rpath1 = strutils_shell_escape (crpath1);
    g_free (mpath1);
    rpath2 = strutils_shell_escape (crpath2);
    g_free (mpath2);
    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILEFROM=%s FISH_FILETO=%s;\n",
                                  SUP.scr_hardlink, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath1, rpath2);
    g_free (shell_commands);
    g_free (rpath1);
    g_free (rpath2);
    return fish_send_command (me, super2, buf, OPT_FLUSH);
}


/* --------------------------------------------------------------------------------------------- */

static int
fish_symlink (struct vfs_class *me, const char *setto, const char *path)
{
    char *qsetto;
    gchar *shell_commands = NULL;
    char buf[BUF_LARGE];
    const char *crpath;
    char *rpath, *mpath;
    struct vfs_s_super *super;

    mpath = g_strdup (path);
    crpath = vfs_s_get_path_mangle (me, mpath, &super, 0);
    if (crpath == NULL)
    {
        g_free (mpath);
        return -1;
    }
    rpath = strutils_shell_escape (crpath);
    g_free (mpath);

    qsetto = strutils_shell_escape (setto);
    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILEFROM=%s FISH_FILETO=%s;\n",
                                  SUP.scr_ln, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, qsetto, rpath);
    g_free (shell_commands);
    g_free (qsetto);
    g_free (rpath);
    return fish_send_command (me, super, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_chmod (struct vfs_class *me, const char *path, int mode)
{
    gchar *shell_commands = NULL;

    PREFIX;

    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s FISH_FILEMODE=%4.4o;\n",
                                  SUP.scr_chmod, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath, mode & 07777);
    g_free (shell_commands);
    g_free (rpath);
    return fish_send_command (me, super, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_chown (struct vfs_class *me, const char *path, uid_t owner, gid_t group)
{
    char *sowner, *sgroup;
    struct passwd *pw;
    struct group *gr;

    pw = getpwuid (owner);
    if (pw == NULL)
        return 0;

    gr = getgrgid (group);
    if (gr == NULL)
        return 0;

    sowner = pw->pw_name;
    sgroup = gr->gr_name;
    {
        gchar *shell_commands = NULL;

        PREFIX;

        shell_commands = g_strconcat (SUP.scr_env,
                                      "FISH_FILENAME=%s FISH_FILEOWNER=%s FISH_FILEGROUP=%s;\n",
                                      SUP.scr_chown, (char *) NULL);
        g_snprintf (buf, sizeof (buf), shell_commands, rpath, sowner, sgroup);
        g_free (shell_commands);
        fish_send_command (me, super, buf, OPT_FLUSH);
        /* FIXME: what should we report if chgrp succeeds but chown fails? */
        /* fish_send_command(me, super, buf, OPT_FLUSH); */
        g_free (rpath);
        return fish_send_command (me, super, buf, OPT_FLUSH);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_utime (struct vfs_class *me, const char *path, struct utimbuf *times)
{
    gchar *shell_commands = NULL;
    char utcmtime[16], utcatime[16];
    struct tm *gmt;

    PREFIX;

    gmt = gmtime(&times->modtime);
    g_snprintf (utcmtime, sizeof (utcmtime), "%04d%02d%02d%02d%02d.%02d",
                gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);

    gmt = gmtime(&times->actime);
    g_snprintf (utcatime, sizeof (utcatime), "%04d%02d%02d%02d%02d.%02d",
                gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);

    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s FISH_FILEATIME=%ld FISH_FILEMTIME=%ld ",
                                  "FISH_TOUCHATIME=%s FISH_TOUCHMTIME=%s;\n",
                                  SUP.scr_utime, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands,
                rpath, (long)times->actime, (long)times->modtime, utcatime, utcmtime);
    g_free (shell_commands);
    g_free (rpath);
    return fish_send_command (me, super, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_unlink (struct vfs_class *me, const char *path)
{
    gchar *shell_commands = NULL;

    PREFIX;

    shell_commands =
        g_strconcat (SUP.scr_env, "FISH_FILENAME=%s;\n", SUP.scr_unlink, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath);
    g_free (shell_commands);
    g_free (rpath);
    return fish_send_command (me, super, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_exists (struct vfs_class *me, const char *path)
{
    gchar *shell_commands = NULL;

    PREFIX;

    shell_commands =
        g_strconcat (SUP.scr_env, "FISH_FILENAME=%s;\n", SUP.scr_exists, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath);
    g_free (shell_commands);
    g_free (rpath);

    return (fish_send_command (me, super, buf, OPT_FLUSH) == 0) ? 1 : 0;
}


/* --------------------------------------------------------------------------------------------- */

static int
fish_mkdir (struct vfs_class *me, const char *path, mode_t mode)
{
    gchar *shell_commands = NULL;
    int ret_code;

    PREFIX;

    (void) mode;

    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s;\n", SUP.scr_mkdir, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath);
    g_free (shell_commands);

    g_free (rpath);
    ret_code = fish_send_command (me, super, buf, OPT_FLUSH);

    if (ret_code != 0)
        return ret_code;

    if (!fish_exists (me, path))
    {
        ERRNOR (EACCES, -1);
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_rmdir (struct vfs_class *me, const char *path)
{
    gchar *shell_commands = NULL;

    PREFIX;

    shell_commands = g_strconcat (SUP.scr_env, "FISH_FILENAME=%s;\n", SUP.scr_rmdir, (char *) NULL);
    g_snprintf (buf, sizeof (buf), shell_commands, rpath);
    g_free (shell_commands);
    g_free (rpath);
    return fish_send_command (me, super, buf, OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
fish_fh_open (struct vfs_class *me, struct vfs_s_fh *fh, int flags, mode_t mode)
{
    (void) mode;

    fh->u.fish.append = 0;
    /* File will be written only, so no need to retrieve it */
    if (((flags & O_WRONLY) == O_WRONLY) && !(flags & (O_RDONLY | O_RDWR)))
    {
        fh->u.fish.append = flags & O_APPEND;
        if (!fh->ino->localname)
        {
            int tmp_handle = vfs_mkstemps (&fh->ino->localname, me->name,
                                           fh->ino->ent->name);
            if (tmp_handle == -1)
                return -1;
            close (tmp_handle);
        }
        return 0;
    }
    if (!fh->ino->localname)
        if (vfs_s_retrieve_file (me, fh->ino) == -1)
            return -1;
    if (!fh->ino->localname)
        vfs_die ("retrieve_file failed to fill in localname");
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
fish_fill_names (struct vfs_class *me, fill_names_f func)
{
    struct vfs_s_super *super = MEDATA->supers;
    char *name;

    char gbuf[10];

    while (super)
    {
        const char *flags = "";
        switch (SUP.flags)
        {
        case FISH_FLAG_RSH:
            flags = ":r";
            break;
        case FISH_FLAG_COMPRESSED:
            flags = ":C";
            break;
        default:
            if (SUP.flags > FISH_FLAG_RSH)
            {
                g_snprintf (gbuf, sizeof (gbuf), ":%d", SUP.flags);
                flags = gbuf;
            }
            break;
        }

        name = g_strconcat ("/#sh:", SUP.user, "@", SUP.host, flags, "/", SUP.cwdir, (char *) NULL);
        (*func) (name);
        g_free (name);
        super = super->next;
    }
}

/* --------------------------------------------------------------------------------------------- */

static void *
fish_open (struct vfs_class *me, const char *file, int flags, mode_t mode)
{
    /*
       sorry, i've places hack here
       cause fish don't able to open files with O_EXCL flag
     */
    flags &= ~O_EXCL;
    return vfs_s_open (me, file, flags, mode);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
init_fish (void)
{
    static struct vfs_s_subclass fish_subclass;

    tcp_init ();

    fish_subclass.flags = VFS_S_REMOTE;
    fish_subclass.archive_same = fish_archive_same;
    fish_subclass.open_archive = fish_open_archive;
    fish_subclass.free_archive = fish_free_archive;
    fish_subclass.fh_open = fish_fh_open;
    fish_subclass.dir_load = fish_dir_load;
    fish_subclass.file_store = fish_file_store;
    fish_subclass.linear_start = fish_linear_start;
    fish_subclass.linear_read = fish_linear_read;
    fish_subclass.linear_close = fish_linear_close;

    vfs_s_init_class (&vfs_fish_ops, &fish_subclass);
    vfs_fish_ops.name = "fish";
    vfs_fish_ops.prefix = "sh:";
    vfs_fish_ops.fill_names = fish_fill_names;
    vfs_fish_ops.chmod = fish_chmod;
    vfs_fish_ops.chown = fish_chown;
    vfs_fish_ops.utime = fish_utime;
    vfs_fish_ops.open = fish_open;
    vfs_fish_ops.symlink = fish_symlink;
    vfs_fish_ops.link = fish_link;
    vfs_fish_ops.unlink = fish_unlink;
    vfs_fish_ops.rename = fish_rename;
    vfs_fish_ops.mkdir = fish_mkdir;
    vfs_fish_ops.rmdir = fish_rmdir;
    vfs_fish_ops.ctl = fish_ctl;
    vfs_register_class (&vfs_fish_ops);
}

/* --------------------------------------------------------------------------------------------- */
