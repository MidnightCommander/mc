/*
   Virtual File System: SHELL implementation for transferring files over
   shell connections.

   Copyright (C) 1998-2024
   Free Software Foundation, Inc.

   Written by:
   Pavel Machek, 1998
   Michal Svec, 2000
   Andrew Borodin <aborodin@vmail.ru>, 2010-2022
   Slava Zanko <slavazanko@gmail.com>, 2010, 2013
   Ilia Maslakov <il.smind@gmail.com>, 2010

   Derived from ftpfs.c.

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

/**
 * \file
 * \brief Source: Virtual File System: SHELL implementation for transferring files over
 * shell connections
 * \author Pavel Machek
 * \author Michal Svec
 * \date 1998, 2000
 *
 * Derived from ftpfs.c
 * Read README.shell for protocol specification.
 *
 * Syntax of path is: \verbatim sh://user@host[:Cr]/path \endverbatim
 *      where C means you want compressed connection,
 *      and r means you want to use rsh
 *
 * Namespace: shell_vfs_ops exported.
 */

/* Define this if your ssh can take -I option */

#include <config.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/tty/tty.h"        /* enable/disable interrupt key */
#include "lib/strutil.h"
#include "lib/unixcompat.h"
#include "lib/fileloc.h"
#include "lib/util.h"           /* my_exit() */
#include "lib/mcconfig.h"

#include "src/execute.h"        /* pre_exec, post_exec */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/netutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_stamp_create */

#include "shell.h"
#include "shelldef.h"

/*** global variables ****************************************************************************/

int shell_directory_timeout = 900;

/*** file scope macro definitions ****************************************************************/

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

#define SHELL_FLAG_COMPRESSED 1
#define SHELL_FLAG_RSH        2

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
#define SHELL_HAVE_HEAD        1
#define SHELL_HAVE_SED         2
#define SHELL_HAVE_AWK         4
#define SHELL_HAVE_PERL        8
#define SHELL_HAVE_LSQ        16
#define SHELL_HAVE_DATE_MDYT  32
#define SHELL_HAVE_TAIL       64

#define SHELL_SUPER(super) ((shell_super_t *) (super))
#define SHELL_FILE_HANDLER(fh) ((shell_file_handler_t *) fh)

/*** file scope type declarations ****************************************************************/

typedef struct
{
    struct vfs_s_super base;    /* base class */

    int sockr;
    int sockw;
    char *scr_ls;
    char *scr_chmod;
    char *scr_utime;
    char *scr_exists;
    char *scr_mkdir;
    char *scr_unlink;
    char *scr_chown;
    char *scr_rmdir;
    char *scr_ln;
    char *scr_mv;
    char *scr_hardlink;
    char *scr_get;
    char *scr_send;
    char *scr_append;
    char *scr_info;
    int host_flags;
    GString *scr_env;
} shell_super_t;

typedef struct
{
    vfs_file_handler_t base;    /* base class */

    off_t got;
    off_t total;
    gboolean append;
} shell_file_handler_t;

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

static char reply_str[80];

static struct vfs_s_subclass shell_subclass;
static struct vfs_class *vfs_shell_ops = VFS_CLASS (&shell_subclass);

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
shell_set_blksize (struct stat *s)
{
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    /* redefine block size */
    s->st_blksize = 64 * 1024;  /* FIXME */
#endif
}

/* --------------------------------------------------------------------------------------------- */

static struct stat *
shell_default_stat (struct vfs_class *me)
{
    struct stat *s;

    s = vfs_s_default_stat (me, S_IFDIR | 0755);
    shell_set_blksize (s);
    vfs_adjust_stat (s);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

static char *
shell_load_script_from_file (const char *hostname, const char *script_name, const char *def_content)
{
    char *scr_filename = NULL;
    char *scr_content;
    gsize scr_len = 0;

    /* 1st: scan user directory */
    scr_filename =
        g_build_path (PATH_SEP_STR, mc_config_get_data_path (), VFS_SHELL_PREFIX, hostname,
                      script_name, (char *) NULL);
    /* silent about user dir */
    g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
    g_free (scr_filename);
    /* 2nd: scan system dir */
    if (scr_content == NULL)
    {
        scr_filename =
            g_build_path (PATH_SEP_STR, LIBEXECDIR, VFS_SHELL_PREFIX, script_name, (char *) NULL);
        g_file_get_contents (scr_filename, &scr_content, &scr_len, NULL);
        g_free (scr_filename);
    }

    if (scr_content != NULL)
        return scr_content;

    return g_strdup (def_content);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_decode_reply (char *s, gboolean was_garbage)
{
    int code;

    /* cppcheck-suppress invalidscanf */
    if (sscanf (s, "%d", &code) == 0)
    {
        code = 500;
        return 5;
    }
    if (code < 100)
        return was_garbage ? ERROR : (code == 0 ? COMPLETE : PRELIM);
    return code / 100;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */

static int
shell_get_reply (struct vfs_class *me, int sock, char *string_buf, int string_len)
{
    char answer[BUF_1K];
    gboolean was_garbage = FALSE;

    while (TRUE)
    {
        if (!vfs_s_get_line (me, sock, answer, sizeof (answer), '\n'))
        {
            if (string_buf != NULL)
                *string_buf = '\0';
            return 4;
        }

        if (strncmp (answer, "### ", 4) == 0)
            return shell_decode_reply (answer + 4, was_garbage ? 1 : 0);

        was_garbage = TRUE;
        if (string_buf != NULL)
            g_strlcpy (string_buf, answer, string_len);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_command (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *cmd,
               size_t cmd_len)
{
    ssize_t status;
    FILE *logfile = me->logfile;

    if (cmd_len == (size_t) (-1))
        cmd_len = strlen (cmd);

    if (logfile != NULL)
    {
        size_t ret;

        ret = fwrite (cmd, cmd_len, 1, logfile);
        ret = fflush (logfile);
        (void) ret;
    }

    tty_enable_interrupt_key ();
    status = write (SHELL_SUPER (super)->sockw, cmd, cmd_len);
    tty_disable_interrupt_key ();

    if (status < 0)
        return TRANSIENT;

    if (wait_reply)
        return shell_get_reply (me, SHELL_SUPER (super)->sockr,
                                (wait_reply & WANT_STRING) != 0 ? reply_str : NULL,
                                sizeof (reply_str) - 1);
    return COMPLETE;
}

/* --------------------------------------------------------------------------------------------- */

static int
G_GNUC_PRINTF (5, 0)
shell_command_va (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *scr,
                  const char *vars, va_list ap)
{
    int r;
    GString *command;

    command = mc_g_string_dup (SHELL_SUPER (super)->scr_env);
    g_string_append_vprintf (command, vars, ap);
    g_string_append (command, scr);
    r = shell_command (me, super, wait_reply, command->str, command->len);
    g_string_free (command, TRUE);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
G_GNUC_PRINTF (5, 6)
shell_command_v (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *scr,
                 const char *vars, ...)
{
    int r;
    va_list ap;

    va_start (ap, vars);
    r = shell_command_va (me, super, wait_reply, scr, vars, ap);
    va_end (ap);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
G_GNUC_PRINTF (5, 6)
shell_send_command (struct vfs_class *me, struct vfs_s_super *super, int flags, const char *scr,
                    const char *vars, ...)
{
    int r;
    va_list ap;

    va_start (ap, vars);
    r = shell_command_va (me, super, WAIT_REPLY, scr, vars, ap);
    va_end (ap);
    vfs_stamp_create (vfs_shell_ops, super);

    if (r != COMPLETE)
        ERRNOR (E_REMOTE, -1);
    if ((flags & OPT_FLUSH) != 0)
        vfs_s_invalidate (me, super);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
shell_new_archive (struct vfs_class *me)
{
    shell_super_t *arch;

    arch = g_new0 (shell_super_t, 1);
    arch->base.me = me;

    return VFS_SUPER (arch);
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    shell_super_t *shell_super = SHELL_SUPER (super);

    if ((shell_super->sockw != -1) || (shell_super->sockr != -1))
        vfs_print_message (_("shell: Disconnecting from %s"), super->name ? super->name : "???");

    if (shell_super->sockw != -1)
    {
        shell_command (me, super, NONE, "exit\n", -1);
        close (shell_super->sockw);
        shell_super->sockw = -1;
    }

    if (shell_super->sockr != -1)
    {
        close (shell_super->sockr);
        shell_super->sockr = -1;
    }

    g_free (shell_super->scr_ls);
    g_free (shell_super->scr_exists);
    g_free (shell_super->scr_mkdir);
    g_free (shell_super->scr_unlink);
    g_free (shell_super->scr_chown);
    g_free (shell_super->scr_chmod);
    g_free (shell_super->scr_utime);
    g_free (shell_super->scr_rmdir);
    g_free (shell_super->scr_ln);
    g_free (shell_super->scr_mv);
    g_free (shell_super->scr_hardlink);
    g_free (shell_super->scr_get);
    g_free (shell_super->scr_send);
    g_free (shell_super->scr_append);
    g_free (shell_super->scr_info);
    g_string_free (shell_super->scr_env, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_pipeopen (struct vfs_s_super *super, const char *path, const char *argv[])
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
        SHELL_SUPER (super)->sockw = fileset1[1];
        close (fileset2[1]);
        SHELL_SUPER (super)->sockr = fileset2[0];
    }
    else
    {
        res = dup2 (fileset1[0], STDIN_FILENO);
        close (fileset1[0]);
        close (fileset1[1]);
        res = dup2 (fileset2[1], STDOUT_FILENO);
        close (STDERR_FILENO);
        /* stderr to /dev/null */
        res = open ("/dev/null", O_WRONLY);
        close (fileset2[0]);
        close (fileset2[1]);
        execvp (path, (char **) argv);
        my_exit (3);
    }
}

/* --------------------------------------------------------------------------------------------- */

static GString *
shell_set_env (int flags)
{
    GString *ret;

    ret = g_string_sized_new (256);

    if ((flags & SHELL_HAVE_HEAD) != 0)
        g_string_append (ret, "SHELL_HAVE_HEAD=1 export SHELL_HAVE_HEAD; ");

    if ((flags & SHELL_HAVE_SED) != 0)
        g_string_append (ret, "SHELL_HAVE_SED=1 export SHELL_HAVE_SED; ");

    if ((flags & SHELL_HAVE_AWK) != 0)
        g_string_append (ret, "SHELL_HAVE_AWK=1 export SHELL_HAVE_AWK; ");

    if ((flags & SHELL_HAVE_PERL) != 0)
        g_string_append (ret, "SHELL_HAVE_PERL=1 export SHELL_HAVE_PERL; ");

    if ((flags & SHELL_HAVE_LSQ) != 0)
        g_string_append (ret, "SHELL_HAVE_LSQ=1 export SHELL_HAVE_LSQ; ");

    if ((flags & SHELL_HAVE_DATE_MDYT) != 0)
        g_string_append (ret, "SHELL_HAVE_DATE_MDYT=1 export SHELL_HAVE_DATE_MDYT; ");

    if ((flags & SHELL_HAVE_TAIL) != 0)
        g_string_append (ret, "SHELL_HAVE_TAIL=1 export SHELL_HAVE_TAIL; ");

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_info (struct vfs_class *me, struct vfs_s_super *super)
{
    shell_super_t *shell_super = SHELL_SUPER (super);

    if (shell_command (me, super, NONE, shell_super->scr_info, -1) == COMPLETE)
    {
        while (TRUE)
        {
            int res;
            char buffer[BUF_8K] = "";

            res = vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), shell_super->sockr);
            if ((res == 0) || (res == EINTR))
                ERRNOR (ECONNRESET, FALSE);
            if (strncmp (buffer, "### ", 4) == 0)
                break;
            shell_super->host_flags = atol (buffer);
        }
        return TRUE;
    }
    ERRNOR (E_PROTO, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_open_archive_pipeopen (struct vfs_s_super *super)
{
    char gbuf[10];
    const char *argv[10];       /* All of 10 is used now */
    const char *xsh = (super->path_element->port == SHELL_FLAG_RSH ? "rsh" : "ssh");
    int i = 0;

    argv[i++] = xsh;
    if (super->path_element->port == SHELL_FLAG_COMPRESSED)
        argv[i++] = "-C";

    if (super->path_element->port > SHELL_FLAG_RSH)
    {
        argv[i++] = "-p";
        g_snprintf (gbuf, sizeof (gbuf), "%d", super->path_element->port);
        argv[i++] = gbuf;
    }

    /*
     * Add the user name to the ssh command line only if it was explicitly
     * set in vfs URL. rsh/ssh will get current user by default
     * plus we can set convenient overrides in  ~/.ssh/config (explicit -l
     * option breaks it for some)
     */

    if (super->path_element->user != NULL)
    {
        argv[i++] = "-l";
        argv[i++] = super->path_element->user;
    }
    else
    {
        /* The rest of the code assumes it to be a valid username */
        super->path_element->user = vfs_get_local_username ();
    }

    argv[i++] = super->path_element->host;
    argv[i++] = "echo SHELL:; /bin/sh";
    argv[i++] = NULL;

    shell_pipeopen (super, xsh, argv);
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
shell_open_archive_talk (struct vfs_class *me, struct vfs_s_super *super)
{
    shell_super_t *shell_super = SHELL_SUPER (super);
    char answer[2048];

    printf ("\n%s\n", _("shell: Waiting for initial line..."));

    if (vfs_s_get_line (me, shell_super->sockr, answer, sizeof (answer), ':') == 0)
        return FALSE;

    if (strstr (answer, "assword") != NULL)
    {
        /* Currently, this does not work. ssh reads passwords from
           /dev/tty, not from stdin :-(. */

        printf ("\n%s\n", _("Sorry, we cannot do password authenticated connections for now."));

        return FALSE;
#if 0
        if (super->path_element->password == NULL)
        {
            char *p, *op;

            p = g_strdup_printf (_("shell: Password is required for %s"),
                                 super->path_element->user);
            op = vfs_get_password (p);
            g_free (p);
            if (op == NULL)
                return FALSE;
            super->path_element->password = op;
        }

        printf ("\n%s\n", _("shell: Sending password..."));

        {
            size_t str_len;

            str_len = strlen (super->path_element->password);
            if ((write (shell_super.sockw, super->path_element->password, str_len) !=
                 (ssize_t) str_len) || (write (shell_super->sockw, "\n", 1) != 1))
                return FALSE;
        }
#endif
    }
    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_open_archive_int (struct vfs_class *me, struct vfs_s_super *super)
{
    gboolean ftalk;

    /* hide panels */
    pre_exec ();

    /* open pipe */
    shell_open_archive_pipeopen (super);

    /* Start talk with ssh-server (password prompt, etc ) */
    ftalk = shell_open_archive_talk (me, super);

    /* show panels */
    post_exec ();

    if (!ftalk)
        ERRNOR (E_PROTO, -1);

    vfs_print_message ("%s", _("shell: Sending initial line..."));

    /* Set up remote locale to C, otherwise dates cannot be recognized */
    if (shell_command
        (me, super, WAIT_REPLY,
         "LANG=C LC_ALL=C LC_TIME=C; export LANG LC_ALL LC_TIME;\n" "echo '### 200'\n",
         -1) != COMPLETE)
        ERRNOR (E_PROTO, -1);

    vfs_print_message ("%s", _("shell: Getting host info..."));
    if (shell_info (me, super))
        SHELL_SUPER (super)->scr_env = shell_set_env (SHELL_SUPER (super)->host_flags);

#if 0
    super->name =
        g_strconcat ("sh://", super->path_element->user, "@", super->path_element->host,
                     PATH_SEP_STR, (char *) NULL);
#else
    super->name = g_strdup (PATH_SEP_STR);
#endif

    super->root = vfs_s_new_inode (me, super, shell_default_stat (me));

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_open_archive (struct vfs_s_super *super,
                    const vfs_path_t *vpath, const vfs_path_element_t *vpath_element)
{
    shell_super_t *shell_super = SHELL_SUPER (super);

    (void) vpath;

    super->path_element = vfs_path_element_clone (vpath_element);

    if (strncmp (vpath_element->vfs_prefix, "rsh", 3) == 0)
        super->path_element->port = SHELL_FLAG_RSH;

    shell_super->scr_ls =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_LS_FILE,
                                     VFS_SHELL_LS_DEF_CONTENT);
    shell_super->scr_exists =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_EXISTS_FILE,
                                     VFS_SHELL_EXISTS_DEF_CONTENT);
    shell_super->scr_mkdir =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_MKDIR_FILE,
                                     VFS_SHELL_MKDIR_DEF_CONTENT);
    shell_super->scr_unlink =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_UNLINK_FILE,
                                     VFS_SHELL_UNLINK_DEF_CONTENT);
    shell_super->scr_chown =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_CHOWN_FILE,
                                     VFS_SHELL_CHOWN_DEF_CONTENT);
    shell_super->scr_chmod =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_CHMOD_FILE,
                                     VFS_SHELL_CHMOD_DEF_CONTENT);
    shell_super->scr_utime =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_UTIME_FILE,
                                     VFS_SHELL_UTIME_DEF_CONTENT);
    shell_super->scr_rmdir =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_RMDIR_FILE,
                                     VFS_SHELL_RMDIR_DEF_CONTENT);
    shell_super->scr_ln =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_LN_FILE,
                                     VFS_SHELL_LN_DEF_CONTENT);
    shell_super->scr_mv =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_MV_FILE,
                                     VFS_SHELL_MV_DEF_CONTENT);
    shell_super->scr_hardlink =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_HARDLINK_FILE,
                                     VFS_SHELL_HARDLINK_DEF_CONTENT);
    shell_super->scr_get =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_GET_FILE,
                                     VFS_SHELL_GET_DEF_CONTENT);
    shell_super->scr_send =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_SEND_FILE,
                                     VFS_SHELL_SEND_DEF_CONTENT);
    shell_super->scr_append =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_APPEND_FILE,
                                     VFS_SHELL_APPEND_DEF_CONTENT);
    shell_super->scr_info =
        shell_load_script_from_file (super->path_element->host, VFS_SHELL_INFO_FILE,
                                     VFS_SHELL_INFO_DEF_CONTENT);

    return shell_open_archive_int (vpath_element->class, super);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_archive_same (const vfs_path_element_t *vpath_element, struct vfs_s_super *super,
                    const vfs_path_t *vpath, void *cookie)
{
    vfs_path_element_t *path_element;
    int result;

    (void) vpath;
    (void) cookie;

    path_element = vfs_path_element_clone (vpath_element);

    if (path_element->user == NULL)
        path_element->user = vfs_get_local_username ();

    result = ((strcmp (path_element->host, super->path_element->host) == 0)
              && (strcmp (path_element->user, super->path_element->user) == 0)
              && (path_element->port == super->path_element->port)) ? 1 : 0;

    vfs_path_element_free (path_element);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_parse_ls (char *buffer, struct vfs_s_entry *ent)
{
#define ST ent->ino->st

    buffer++;

    switch (buffer[-1])
    {
    case ':':
        {
            char *filename;
            char *filename_bound;
            char *temp;

            filename = buffer;

            if (strcmp (filename, "\".\"") == 0 || strcmp (filename, "\"..\"") == 0)
                break;          /* We'll do "." and ".." ourselves */

            filename_bound = filename + strlen (filename);

            if (S_ISLNK (ST.st_mode))
            {
                char *linkname;
                char *linkname_bound;
                /* we expect: "escaped-name" -> "escaped-name"
                   //     -> cannot occur in filenames,
                   //     because it will be escaped to -\> */


                linkname_bound = filename_bound;

                if (*filename == '"')
                    ++filename;

                linkname = strstr (filename, "\" -> \"");
                if (linkname == NULL)
                {
                    /* broken client, or smth goes wrong */
                    linkname = filename_bound;
                    if (filename_bound > filename && *(filename_bound - 1) == '"')
                        --filename_bound;       /* skip trailing " */
                }
                else
                {
                    filename_bound = linkname;
                    linkname += 6;      /* strlen ("\" -> \"") */
                    if (*(linkname_bound - 1) == '"')
                        --linkname_bound;       /* skip trailing " */
                }

                ent->name = g_strndup (filename, filename_bound - filename);
                temp = ent->name;
                ent->name = str_shell_unescape (ent->name);
                g_free (temp);

                ent->ino->linkname = g_strndup (linkname, linkname_bound - linkname);
                temp = ent->ino->linkname;
                ent->ino->linkname = str_shell_unescape (ent->ino->linkname);
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
                ent->name = str_shell_unescape (ent->name);
                g_free (temp);
            }
            break;
        }

    case 'S':
        ST.st_size = (off_t) g_ascii_strtoll (buffer, NULL, 10);
        break;

    case 'P':
        {
            size_t skipped;

            vfs_parse_filemode (buffer, &skipped, &ST.st_mode);
            break;
        }

    case 'R':
        {
            /*
               raw filemode:
               we expect: Roctal-filemode octal-filetype uid.gid
             */
            size_t skipped;

            vfs_parse_raw_filemode (buffer, &skipped, &ST.st_mode);
            break;
        }

    case 'd':
        vfs_split_text (buffer);
        vfs_zero_stat_times (&ST);
        if (vfs_parse_filedate (0, &ST.st_ctime) == 0)
            break;
        ST.st_atime = ST.st_mtime = ST.st_ctime;
        break;

    case 'D':
        {
            struct tm tim;

            memset (&tim, 0, sizeof (tim));
            /* cppcheck-suppress invalidscanf */
            if (sscanf (buffer, "%d %d %d %d %d %d", &tim.tm_year, &tim.tm_mon,
                        &tim.tm_mday, &tim.tm_hour, &tim.tm_min, &tim.tm_sec) != 6)
                break;
            vfs_zero_stat_times (&ST);
            ST.st_atime = ST.st_mtime = ST.st_ctime = mktime (&tim);
        }
        break;

    case 'E':
        {
            int maj, min;

            /* cppcheck-suppress invalidscanf */
            if (sscanf (buffer, "%d,%d", &maj, &min) != 2)
                break;
#ifdef HAVE_STRUCT_STAT_ST_RDEV
            ST.st_rdev = makedev (maj, min);
#endif
        }
        break;

    default:
        break;
    }

#undef ST
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path)
{
    struct vfs_s_super *super = dir->super;
    char buffer[BUF_8K] = "\0";
    struct vfs_s_entry *ent = NULL;
    char *quoted_path;
    int reply_code;

    /*
     * Simple SHELL debug interface :]
     */
#if 0
    if (me->logfile == NULL)
        me->logfile = fopen ("/tmp/mc-SHELL.sh", "w");
#endif

    vfs_print_message (_("shell: Reading directory %s..."), remote_path);

    dir->timestamp = g_get_monotonic_time () + shell_directory_timeout * G_USEC_PER_SEC;

    quoted_path = str_shell_escape (remote_path);
    (void) shell_command_v (me, super, NONE, SHELL_SUPER (super)->scr_ls, "SHELL_FILENAME=%s;\n",
                            quoted_path);
    g_free (quoted_path);

    ent = vfs_s_generate_entry (me, NULL, dir, 0);

    while (TRUE)
    {
        int res;

        res =
            vfs_s_get_line_interruptible (me, buffer, sizeof (buffer), SHELL_SUPER (super)->sockr);

        if ((res == 0) || (res == EINTR))
        {
            vfs_s_free_entry (me, ent);
            me->verrno = ECONNRESET;
            goto error;
        }
        if (me->logfile != NULL)
        {
            fputs (buffer, me->logfile);
            fputs ("\n", me->logfile);
            fflush (me->logfile);
        }
        if (strncmp (buffer, "### ", 4) == 0)
            break;

        if (buffer[0] != '\0')
            shell_parse_ls (buffer, ent);
        else if (ent->name != NULL)
        {
            vfs_s_insert_entry (me, dir, ent);
            ent = vfs_s_generate_entry (me, NULL, dir, 0);
        }
    }

    vfs_s_free_entry (me, ent);
    reply_code = shell_decode_reply (buffer + 4, 0);
    if (reply_code == COMPLETE)
    {
        vfs_print_message (_("%s: done."), me->name);
        return 0;
    }

    me->verrno = reply_code == ERROR ? EACCES : E_REMOTE;

  error:
    vfs_print_message (_("%s: failure"), me->name);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_file_store (struct vfs_class *me, vfs_file_handler_t *fh, char *name, char *localname)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    shell_super_t *shell_super = SHELL_SUPER (super);
    int code;
    off_t total = 0;
    char buffer[BUF_8K];
    struct stat s;
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
     *  If 'head' is not present on the remote system, 'dd' will be used.
     * Unfortunately, we cannot trust most non-GNU 'head' implementations
     * even if '-c' options is supported. Therefore, we separate GNU head
     * (and other modern heads?) using '-q' and '-' . This causes another
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
     *  'dd' was not designed for full filling of input buffers,
     *  and does not report exact number of bytes (not blocks).
     *  Therefore a more complex shell script is needed.
     *
     *   On some systems non-GNU head writes "Usage:" error report to stdout
     *  instead of stderr. It makes impossible the use of "head || dd"
     *  algorithm for file appending case, therefore just "dd" is used for it.
     */

    quoted_name = str_shell_escape (name);
    vfs_print_message (_("shell: store %s: sending command..."), quoted_name);

    /* FIXME: File size is limited to ULONG_MAX */
    code =
        shell_command_v (me, super, WAIT_REPLY,
                         shell->append ? shell_super->scr_append : shell_super->scr_send,
                         "SHELL_FILENAME=%s SHELL_FILESIZE=%" PRIuMAX ";\n", quoted_name,
                         (uintmax_t) s.st_size);
    g_free (quoted_name);

    if (code != PRELIM)
    {
        close (h);
        ERRNOR (E_REMOTE, -1);
    }

    while (TRUE)
    {
        ssize_t n, t;

        while ((n = read (h, buffer, sizeof (buffer))) < 0)
        {
            if ((errno == EINTR) && tty_got_interrupt ())
                continue;
            vfs_print_message ("%s", _("shell: Local read failed, sending zeros"));
            close (h);
            h = open ("/dev/zero", O_RDONLY);
        }

        if (n == 0)
            break;

        t = write (shell_super->sockw, buffer, n);
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
        vfs_print_message ("%s: %" PRIuMAX "/%" PRIuMAX, _("shell: storing file"),
                           (uintmax_t) total, (uintmax_t) s.st_size);
    }
    close (h);

    if (shell_get_reply (me, shell_super->sockr, NULL, 0) != COMPLETE)
        ERRNOR (E_REMOTE, -1);
    return 0;

  error_return:
    close (h);
    shell_get_reply (me, shell_super->sockr, NULL, 0);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_linear_start (struct vfs_class *me, vfs_file_handler_t *fh, off_t offset)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    char *name;
    char *quoted_name;

    name = vfs_s_fullpath (me, fh->ino);
    if (name == NULL)
        return 0;
    quoted_name = str_shell_escape (name);
    g_free (name);
    shell->append = FALSE;

    /*
     * Check whether the remote file is readable by using 'dd' to copy 
     * a single byte from the remote file to /dev/null. If 'dd' completes
     * with exit status of 0 use 'cat' to send the file contents to the
     * standard output (i.e. over the network).
     */

    offset =
        shell_command_v (me, super, WANT_STRING, SHELL_SUPER (super)->scr_get,
                         "SHELL_FILENAME=%s SHELL_START_OFFSET=%" PRIuMAX ";\n", quoted_name,
                         (uintmax_t) offset);
    g_free (quoted_name);

    if (offset != PRELIM)
        ERRNOR (E_REMOTE, 0);
    fh->linear = LS_LINEAR_OPEN;
    shell->got = 0;
    errno = 0;
#if SIZEOF_OFF_T == SIZEOF_LONG
    shell->total = (off_t) strtol (reply_str, NULL, 10);
#else
    shell->total = (off_t) g_ascii_strtoll (reply_str, NULL, 10);
#endif
    if (errno != 0)
        ERRNOR (E_REMOTE, 0);
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_linear_abort (struct vfs_class *me, vfs_file_handler_t *fh)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    char buffer[BUF_8K];
    ssize_t n;

    vfs_print_message ("%s", _("Aborting transfer..."));

    do
    {
        n = MIN ((off_t) sizeof (buffer), (shell->total - shell->got));
        if (n != 0)
        {
            n = read (SHELL_SUPER (super)->sockr, buffer, n);
            if (n < 0)
                return;
            shell->got += n;
        }
    }
    while (n != 0);

    if (shell_get_reply (me, SHELL_SUPER (super)->sockr, NULL, 0) != COMPLETE)
        vfs_print_message ("%s", _("Error reported after abort."));
    else
        vfs_print_message ("%s", _("Aborted transfer would be successful."));
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
shell_linear_read (struct vfs_class *me, vfs_file_handler_t *fh, void *buf, size_t len)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    ssize_t n = 0;

    len = MIN ((size_t) (shell->total - shell->got), len);
    tty_disable_interrupt_key ();
    while (len != 0 && ((n = read (SHELL_SUPER (super)->sockr, buf, len)) < 0))
    {
        if ((errno == EINTR) && !tty_got_interrupt ())
            continue;
        break;
    }
    tty_enable_interrupt_key ();

    if (n > 0)
        shell->got += n;
    else if (n < 0)
        shell_linear_abort (me, fh);
    else if (shell_get_reply (me, SHELL_SUPER (super)->sockr, NULL, 0) != COMPLETE)
        ERRNOR (E_REMOTE, -1);
    ERRNOR (errno, n);
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_linear_close (struct vfs_class *me, vfs_file_handler_t *fh)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);

    if (shell->total != shell->got)
        shell_linear_abort (me, fh);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_ctl (void *fh, int ctlop, void *arg)
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
            vfs_file_handler_t *file = VFS_FILE_HANDLER (fh);
            int v;

            if (file->linear == LS_NOT_LINEAR)
                vfs_die ("You may not do this");
            if (file->linear == LS_LINEAR_CLOSED || file->linear == LS_LINEAR_PREOPEN)
                return 0;

            v = vfs_s_select_on_two (VFS_FILE_HANDLER_SUPER (fh)->u.shell.sockr, 0);

            return (((v < 0) && (errno == EINTR)) || v == 0) ? 1 : 0;
        }
    default:
        return 0;
    }
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    const char *crpath1, *crpath2;
    char *rpath1, *rpath2;
    struct vfs_s_super *super, *super2;
    struct vfs_class *me;
    int ret;

    crpath1 = vfs_s_get_path (vpath1, &super, 0);
    if (crpath1 == NULL)
        return -1;

    crpath2 = vfs_s_get_path (vpath2, &super2, 0);
    if (crpath2 == NULL)
        return -1;

    rpath1 = str_shell_escape (crpath1);
    rpath2 = str_shell_escape (crpath2);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath1));

    ret =
        shell_send_command (me, super2, OPT_FLUSH, SHELL_SUPER (super)->scr_mv,
                            "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", rpath1, rpath2);

    g_free (rpath1);
    g_free (rpath2);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_link (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    const char *crpath1, *crpath2;
    char *rpath1, *rpath2;
    struct vfs_s_super *super, *super2;
    struct vfs_class *me;
    int ret;

    crpath1 = vfs_s_get_path (vpath1, &super, 0);
    if (crpath1 == NULL)
        return -1;

    crpath2 = vfs_s_get_path (vpath2, &super2, 0);
    if (crpath2 == NULL)
        return -1;

    rpath1 = str_shell_escape (crpath1);
    rpath2 = str_shell_escape (crpath2);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath1));

    ret =
        shell_send_command (me, super2, OPT_FLUSH, SHELL_SUPER (super)->scr_hardlink,
                            "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", rpath1, rpath2);

    g_free (rpath1);
    g_free (rpath2);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_symlink (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    char *qsetto;
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath2, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);
    qsetto = str_shell_escape (vfs_path_get_last_path_str (vpath1));

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath2));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_ln,
                            "SHELL_FILEFROM=%s SHELL_FILETO=%s;\n", qsetto, rpath);

    g_free (qsetto);
    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_stat (const vfs_path_t *vpath, struct stat *buf)
{
    int ret;

    ret = vfs_s_stat (vpath, buf);
    shell_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_lstat (const vfs_path_t *vpath, struct stat *buf)
{
    int ret;

    ret = vfs_s_lstat (vpath, buf);
    shell_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_fstat (void *vfs_info, struct stat *buf)
{
    int ret;

    ret = vfs_s_fstat (vfs_info, buf);
    shell_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_chmod (const vfs_path_t *vpath, mode_t mode)
{
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_chmod,
                            "SHELL_FILENAME=%s SHELL_FILEMODE=%4.4o;\n", rpath,
                            (unsigned int) (mode & 07777));

    g_free (rpath);

    return ret;;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_chown (const vfs_path_t *vpath, uid_t owner, gid_t group)
{
    char *sowner, *sgroup;
    struct passwd *pw;
    struct group *gr;
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    pw = getpwuid (owner);
    if (pw == NULL)
        return 0;

    gr = getgrgid (group);
    if (gr == NULL)
        return 0;

    sowner = pw->pw_name;
    sgroup = gr->gr_name;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    /* FIXME: what should we report if chgrp succeeds but chown fails? */
    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_chown,
                            "SHELL_FILENAME=%s SHELL_FILEOWNER=%s SHELL_FILEGROUP=%s;\n", rpath,
                            sowner, sgroup);

    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_utime (const vfs_path_t *vpath, mc_timesbuf_t *times)
{
    char utcatime[16], utcmtime[16];
    char utcatime_w_nsec[30], utcmtime_w_nsec[30];
    mc_timespec_t atime, mtime;
    struct tm *gmt;
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    vfs_get_timespecs_from_timesbuf (times, &atime, &mtime);

    gmt = gmtime (&atime.tv_sec);
    g_snprintf (utcatime, sizeof (utcatime), "%04d%02d%02d%02d%02d.%02d",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    g_snprintf (utcatime_w_nsec, sizeof (utcatime_w_nsec), "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec, atime.tv_nsec);

    gmt = gmtime (&mtime.tv_sec);
    g_snprintf (utcmtime, sizeof (utcmtime), "%04d%02d%02d%02d%02d.%02d",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
    g_snprintf (utcmtime_w_nsec, sizeof (utcmtime_w_nsec), "%04d-%02d-%02d %02d:%02d:%02d.%09ld",
                gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec, mtime.tv_nsec);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret = shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_utime,
                              "SHELL_FILENAME=%s SHELL_FILEATIME=%ju SHELL_FILEMTIME=%ju "
                              "SHELL_TOUCHATIME=%s SHELL_TOUCHMTIME=%s SHELL_TOUCHATIME_W_NSEC=\"%s\" "
                              "SHELL_TOUCHMTIME_W_NSEC=\"%s\";\n", rpath, (uintmax_t) atime.tv_sec,
                              (uintmax_t) mtime.tv_sec, utcatime, utcmtime, utcatime_w_nsec,
                              utcmtime_w_nsec);

    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_unlink (const vfs_path_t *vpath)
{
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_unlink,
                            "SHELL_FILENAME=%s;\n", rpath);

    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_exists (const vfs_path_t *vpath)
{
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_exists,
                            "SHELL_FILENAME=%s;\n", rpath);

    g_free (rpath);

    return (ret == 0 ? 1 : 0);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    (void) mode;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_mkdir,
                            "SHELL_FILENAME=%s;\n", rpath);
    g_free (rpath);

    if (ret != 0)
        return ret;

    if (shell_exists (vpath) == 0)
    {
        me->verrno = EACCES;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_rmdir (const vfs_path_t *vpath)
{
    const char *crpath;
    char *rpath;
    struct vfs_s_super *super;
    struct vfs_class *me;
    int ret;

    crpath = vfs_s_get_path (vpath, &super, 0);
    if (crpath == NULL)
        return -1;

    rpath = str_shell_escape (crpath);

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    ret =
        shell_send_command (me, super, OPT_FLUSH, SHELL_SUPER (super)->scr_rmdir,
                            "SHELL_FILENAME=%s;\n", rpath);

    g_free (rpath);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static vfs_file_handler_t *
shell_fh_new (struct vfs_s_inode *ino, gboolean changed)
{
    shell_file_handler_t *fh;

    fh = g_new0 (shell_file_handler_t, 1);
    vfs_s_init_fh (VFS_FILE_HANDLER (fh), ino, changed);

    return VFS_FILE_HANDLER (fh);
}

/* --------------------------------------------------------------------------------------------- */

static int
shell_fh_open (struct vfs_class *me, vfs_file_handler_t *fh, int flags, mode_t mode)
{
    shell_file_handler_t *shell = SHELL_FILE_HANDLER (fh);

    (void) mode;

    /* File will be written only, so no need to retrieve it */
    if (((flags & O_WRONLY) == O_WRONLY) && ((flags & (O_RDONLY | O_RDWR)) == 0))
    {
        /* user pressed the button [ Append ] in the "Copy" dialog */
        if ((flags & O_APPEND) != 0)
            shell->append = TRUE;

        if (fh->ino->localname == NULL)
        {
            vfs_path_t *vpath = NULL;
            int tmp_handle;

            tmp_handle = vfs_mkstemps (&vpath, me->name, fh->ino->ent->name);
            if (tmp_handle == -1)
                return (-1);

            fh->ino->localname = vfs_path_free (vpath, FALSE);
            close (tmp_handle);
        }
        return 0;
    }

    if (fh->ino->localname == NULL && vfs_s_retrieve_file (me, fh->ino) == -1)
        return (-1);

    if (fh->ino->localname == NULL)
        vfs_die ("retrieve_file failed to fill in localname");
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
shell_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    for (iter = VFS_SUBCLASS (me)->supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;

        char *name;
        char gbuf[10];
        const char *flags = "";

        switch (super->path_element->port)
        {
        case SHELL_FLAG_RSH:
            flags = ":r";
            break;
        case SHELL_FLAG_COMPRESSED:
            flags = ":C";
            break;
        default:
            if (super->path_element->port > SHELL_FLAG_RSH)
            {
                g_snprintf (gbuf, sizeof (gbuf), ":%d", super->path_element->port);
                flags = gbuf;
            }
            break;
        }

        name =
            g_strconcat (vfs_shell_ops->prefix, VFS_PATH_URL_DELIMITER,
                         super->path_element->user, "@", super->path_element->host, flags,
                         PATH_SEP_STR, super->path_element->path, (char *) NULL);
        func (name);
        g_free (name);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void *
shell_open (const vfs_path_t *vpath, int flags, mode_t mode)
{
    /*
       sorry, i've places hack here
       cause shell don't able to open files with O_EXCL flag
     */
    flags &= ~O_EXCL;
    return vfs_s_open (vpath, flags, mode);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
vfs_init_shell (void)
{
    tcp_init ();

    vfs_init_subclass (&shell_subclass, "shell", VFSF_REMOTE | VFSF_USETMP, "sh");
    vfs_shell_ops->fill_names = shell_fill_names;
    vfs_shell_ops->stat = shell_stat;
    vfs_shell_ops->lstat = shell_lstat;
    vfs_shell_ops->fstat = shell_fstat;
    vfs_shell_ops->chmod = shell_chmod;
    vfs_shell_ops->chown = shell_chown;
    vfs_shell_ops->utime = shell_utime;
    vfs_shell_ops->open = shell_open;
    vfs_shell_ops->symlink = shell_symlink;
    vfs_shell_ops->link = shell_link;
    vfs_shell_ops->unlink = shell_unlink;
    vfs_shell_ops->rename = shell_rename;
    vfs_shell_ops->mkdir = shell_mkdir;
    vfs_shell_ops->rmdir = shell_rmdir;
    vfs_shell_ops->ctl = shell_ctl;
    shell_subclass.archive_same = shell_archive_same;
    shell_subclass.new_archive = shell_new_archive;
    shell_subclass.open_archive = shell_open_archive;
    shell_subclass.free_archive = shell_free_archive;
    shell_subclass.fh_new = shell_fh_new;
    shell_subclass.fh_open = shell_fh_open;
    shell_subclass.dir_load = shell_dir_load;
    shell_subclass.file_store = shell_file_store;
    shell_subclass.linear_start = shell_linear_start;
    shell_subclass.linear_read = shell_linear_read;
    shell_subclass.linear_close = shell_linear_close;
    vfs_register_class (vfs_shell_ops);
}

/* --------------------------------------------------------------------------------------------- */
