/*
   Virtual File System: FTP file system.

   Copyright (C) 1995-2024
   Free Software Foundation, Inc.

   Written by:
   Ching Hui, 1995
   Jakub Jelinek, 1995
   Miguel de Icaza, 1995, 1996, 1997
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Yury V. Zaytsev, 2010
   Slava Zanko <slavazanko@gmail.com>, 2010, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2010-2022

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
 * \brief Source: Virtual File System: FTP file system
 * \author Ching Hui
 * \author Jakub Jelinek
 * \author Miguel de Icaza
 * \author Norbert Warmuth
 * \author Pavel Machek
 * \date 1995, 1997, 1998
 *
 * \todo
- make it more robust - all the connects etc. should handle EADDRINUSE and
  ERETRY (have I spelled these names correctly?)
- make the user able to flush a connection - all the caches will get empty
  etc., (tarfs as well), we should give there a user selectable timeout
  and assign a key sequence.
- use hash table instead of linklist to cache ftpfs directory.

What to do with this?


     * NOTE: Usage of tildes is deprecated, consider:
     * \verbatim
         cd ftp//:pavel@hobit
         cd ~
       \endverbatim
     * And now: what do I want to do? Do I want to go to /home/pavel or to
     * ftp://hobit/home/pavel? I think first has better sense...
     *
    \verbatim
    {
        int f = !strcmp( remote_path, "/~" );
        if (f || !strncmp( remote_path, "/~/", 3 )) {
            char *s;
            s = mc_build_filename ( qhome (*bucket), remote_path +3-f, (char *) NULL );
            g_free (remote_path);
            remote_path = s;
        }
    }
    \endverbatim
 */

/* \todo Fix: Namespace pollution: horrible */

#include <config.h>
#include <stdio.h>              /* sscanf() */
#include <stdlib.h>             /* atoi() */
#include <sys/types.h>          /* POSIX-required by sys/socket.h and netdb.h */
#include <netdb.h>              /* struct hostent */
#include <sys/socket.h>         /* AF_INET */
#include <netinet/in.h>         /* struct in_addr */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <arpa/ftp.h>
#include <arpa/telnet.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/file-entry.h"
#include "lib/util.h"
#include "lib/strutil.h"        /* str_move() */
#include "lib/mcconfig.h"

#include "lib/tty/tty.h"        /* enable/disable interrupt key */
#include "lib/widget.h"         /* message() */

#include "src/history.h"
#include "src/setup.h"          /* for load_anon_passwd */

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"
#include "lib/vfs/netutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/gc.h"         /* vfs_stamp_create */

#include "ftpfs.h"

/*** global variables ****************************************************************************/

/* Delay to retry a connection */
int ftpfs_retry_seconds = 30;

/* Method to use to connect to ftp sites */
gboolean ftpfs_use_passive_connections = TRUE;
gboolean ftpfs_use_passive_connections_over_proxy = FALSE;

/* Method used to get directory listings:
 * 1: try 'LIST -la <path>', if it fails
 *    fall back to CWD <path>; LIST
 * 0: always use CWD <path>; LIST
 */
gboolean ftpfs_use_unix_list_options = TRUE;

/* First "CWD <path>", then "LIST -la ." */
gboolean ftpfs_first_cd_then_ls = TRUE;

/* Use the ~/.netrc */
gboolean ftpfs_use_netrc = TRUE;

/* Anonymous setup */
char *ftpfs_anonymous_passwd = NULL;
int ftpfs_directory_timeout = 900;

/* Proxy host */
char *ftpfs_proxy_host = NULL;

/* whether we have to use proxy by default? */
gboolean ftpfs_always_use_proxy = FALSE;

gboolean ftpfs_ignore_chattr_errors = TRUE;

/*** file scope macro definitions ****************************************************************/

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define FTP_SUPER(super) ((ftp_super_t *) (super))
#define FTP_FILE_HANDLER(fh) ((ftp_file_handler_t *) (fh))
#define FH_SOCK FTP_FILE_HANDLER(fh)->sock

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#define RFC_AUTODETECT 0
#define RFC_DARING 1
#define RFC_STRICT 2

/* ftpfs_command wait_flag: */
#define NONE        0x00
#define WAIT_REPLY  0x01
#define WANT_STRING 0x02

#define FTP_COMMAND_PORT   21

/* some defines only used by ftpfs_changetype */
/* These two are valid values for the second parameter */
#define TYPE_ASCII    0
#define TYPE_BINARY   1

/* This one is only used to initialize bucket->isbinary, don't use it as
   second parameter to ftpfs_changetype. */
#define TYPE_UNKNOWN -1

#define ABORT_TIMEOUT (5 * G_USEC_PER_SEC)
/*** file scope type declarations ****************************************************************/

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/* This should match the keywords[] array below */
typedef enum
{
    NETRC_NONE = 0,
    NETRC_DEFAULT,
    NETRC_MACHINE,
    NETRC_LOGIN,
    NETRC_PASSWORD,
    NETRC_PASSWD,
    NETRC_ACCOUNT,
    NETRC_MACDEF,
    NETRC_UNKNOWN
} keyword_t;

typedef struct
{
    struct vfs_s_super base;    /* base class */

    int sock;

    char *proxy;                /* proxy server, NULL if no proxy */
    gboolean failed_on_login;   /* used to pass the failure reason to upper levels */
    gboolean use_passive_connection;
    gboolean remote_is_amiga;   /* No leading slash allowed for AmiTCP (Amiga) */
    int isbinary;
    gboolean cwd_deferred;      /* current_directory was changed but CWD command hasn't
                                   been sent yet */
    int strict;                 /* ftp server doesn't understand
                                 * "LIST -la <path>"; use "CWD <path>"/
                                 * "LIST" instead
                                 */
    gboolean ctl_connection_busy;
    char *current_dir;
} ftp_super_t;

typedef struct
{
    vfs_file_handler_t base;    /* base class */

    int sock;
    gboolean append;
} ftp_file_handler_t;

/*** forward declarations (file scope functions) *************************************************/

static char *ftpfs_get_current_directory (struct vfs_class *me, struct vfs_s_super *super);
static int ftpfs_chdir_internal (struct vfs_class *me, struct vfs_s_super *super,
                                 const char *remote_path);
static int ftpfs_open_socket (struct vfs_class *me, struct vfs_s_super *super);
static gboolean ftpfs_login_server (struct vfs_class *me, struct vfs_s_super *super,
                                    const char *netrcpass);
static gboolean ftpfs_netrc_lookup (const char *host, char **login, char **pass);

/*** file scope variables ************************************************************************/

static int code;

static char reply_str[80];

static struct vfs_s_subclass ftpfs_subclass;
static struct vfs_class *vfs_ftpfs_ops = VFS_CLASS (&ftpfs_subclass);

static GSList *no_proxy = NULL;

static char buffer[BUF_MEDIUM];
static char *netrc = NULL;
static const char *netrcp;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_set_blksize (struct stat *s)
{
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    /* redefine block size */
    s->st_blksize = 64 * 1024;  /* FIXME */
#endif
}

/* --------------------------------------------------------------------------------------------- */

static struct stat *
ftpfs_default_stat (struct vfs_class *me)
{
    struct stat *s;

    s = vfs_s_default_stat (me, S_IFDIR | 0755);
    ftpfs_set_blksize (s);
    vfs_adjust_stat (s);

    return s;
}

/* --------------------------------------------------------------------------------------------- */

/* Translate a Unix path, i.e. MC's internal path representation (e.g.
   /somedir/somefile) to a path valid for the remote server. Every path
   transferred to the remote server has to be mangled by this function
   right prior to sending it.
   Currently only Amiga ftp servers are handled in a special manner.

   When the remote server is an amiga:
   a) strip leading slash if necessary
   b) replace first occurrence of ":/" with ":"
   c) strip trailing "/."
 */
static char *
ftpfs_translate_path (struct vfs_class *me, struct vfs_s_super *super, const char *remote_path)
{
    char *ret, *p;

    if (!FTP_SUPER (super)->remote_is_amiga)
        return g_strdup (remote_path);

    if (me->logfile != NULL)
    {
        fprintf (me->logfile, "MC -- ftpfs_translate_path: %s\n", remote_path);
        fflush (me->logfile);
    }

    /* strip leading slash(es) */
    while (IS_PATH_SEP (*remote_path))
        remote_path++;

    /* Don't change "/" into "", e.g. "CWD " would be invalid. */
    if (*remote_path == '\0')
        return g_strdup (".");

    ret = g_strdup (remote_path);

    /* replace first occurrence of ":/" with ":" */
    p = strchr (ret, ':');
    if (p != NULL && IS_PATH_SEP (p[1]))
        str_move (p + 1, p + 2);

    /* strip trailing "/." */
    p = strrchr (ret, PATH_SEP);
    if ((p != NULL) && (*(p + 1) == '.') && (*(p + 2) == '\0'))
        *p = '\0';

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/** Extract the hostname and username from the path */
/*
 * path is in the form: [user@]hostname:port/remote-dir, e.g.:
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * If the user is empty, e.g. ftp://@roxanne/private, then your login name
 * is supplied.
 */

static vfs_path_element_t *
ftpfs_correct_url_parameters (const vfs_path_element_t *velement)
{
    vfs_path_element_t *path_element = vfs_path_element_clone (velement);

    if (path_element->port == 0)
        path_element->port = FTP_COMMAND_PORT;

    if (path_element->user == NULL)
    {
        /* Look up user and password in netrc */
        if (ftpfs_use_netrc)
            ftpfs_netrc_lookup (path_element->host, &path_element->user, &path_element->password);
    }
    if (path_element->user == NULL)
        path_element->user = g_strdup ("anonymous");

    /* Look up password in netrc for known user */
    if (ftpfs_use_netrc && path_element->password == NULL)
    {
        char *new_user = NULL;
        char *new_passwd = NULL;

        ftpfs_netrc_lookup (path_element->host, &new_user, &new_passwd);

        /* If user is different, remove password */
        if (new_user != NULL && strcmp (path_element->user, new_user) != 0)
            MC_PTR_FREE (path_element->password);

        g_free (new_user);
        g_free (new_passwd);
    }

    return path_element;
}

/* --------------------------------------------------------------------------------------------- */
/* Returns a reply code, check /usr/include/arpa/ftp.h for possible values */

static int
ftpfs_get_reply (struct vfs_class *me, int sock, char *string_buf, int string_len)
{
    while (TRUE)
    {
        char answer[BUF_1K];

        if (vfs_s_get_line (me, sock, answer, sizeof (answer), '\n') == 0)
        {
            if (string_buf != NULL)
                *string_buf = '\0';
            code = 421;
            return 4;
        }

        /* cppcheck-suppress invalidscanf */
        switch (sscanf (answer, "%d", &code))
        {
        case 0:
            if (string_buf != NULL)
                g_strlcpy (string_buf, answer, string_len);
            code = 500;
            return 5;
        case 1:
            if (answer[3] == '-')
            {
                while (TRUE)
                {
                    int i;

                    if (vfs_s_get_line (me, sock, answer, sizeof (answer), '\n') == 0)
                    {
                        if (string_buf != NULL)
                            *string_buf = '\0';
                        code = 421;
                        return 4;
                    }
                    /* cppcheck-suppress invalidscanf */
                    if ((sscanf (answer, "%d", &i) > 0) && (code == i) && (answer[3] == ' '))
                        break;
                }
            }
            if (string_buf != NULL)
                g_strlcpy (string_buf, answer, string_len);
            return code / 100;
        default:
            break;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftpfs_reconnect (struct vfs_class *me, struct vfs_s_super *super)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int sock;

    sock = ftpfs_open_socket (me, super);
    if (sock != -1)
    {
        char *cwdir = ftp_super->current_dir;

        close (ftp_super->sock);
        ftp_super->sock = sock;
        ftp_super->current_dir = NULL;

        if (ftpfs_login_server (me, super, super->path_element->password))
        {
            if (cwdir == NULL)
                return TRUE;

            sock = ftpfs_chdir_internal (me, super, cwdir);
            g_free (cwdir);
            return (sock == COMPLETE);
        }

        ftp_super->current_dir = cwdir;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */

static int
G_GNUC_PRINTF (4, 5)
ftpfs_command (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *fmt,
               ...)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    va_list ap;
    GString *cmdstr;
    int status;
    static gboolean retry = FALSE;
    static int level = 0;       /* ftpfs_login_server() use ftpfs_command() */

    cmdstr = g_string_sized_new (32);
    va_start (ap, fmt);
    g_string_vprintf (cmdstr, fmt, ap);
    va_end (ap);
    g_string_append (cmdstr, "\r\n");

    if (me->logfile != NULL)
    {
        if (strncmp (cmdstr->str, "PASS ", 5) == 0)
            fputs ("PASS <Password not logged>\r\n", me->logfile);
        else
        {
            size_t ret;

            ret = fwrite (cmdstr->str, cmdstr->len, 1, me->logfile);
            (void) ret;
        }

        fflush (me->logfile);
    }

    got_sigpipe = 0;
    tty_enable_interrupt_key ();
    status = write (ftp_super->sock, cmdstr->str, cmdstr->len);

    if (status < 0)
    {
        code = 421;

        if (errno == EPIPE)
        {                       /* Remote server has closed connection */
            if (level == 0)
            {
                level = 1;
                status = ftpfs_reconnect (me, super) ? 1 : 0;
                level = 0;
                if (status != 0 && (write (ftp_super->sock, cmdstr->str, cmdstr->len) > 0))
                    goto ok;

            }
            got_sigpipe = 1;
        }
        g_string_free (cmdstr, TRUE);
        tty_disable_interrupt_key ();
        return TRANSIENT;
    }

    retry = FALSE;

  ok:
    tty_disable_interrupt_key ();

    if (wait_reply != NONE)
    {
        status = ftpfs_get_reply (me, ftp_super->sock,
                                  (wait_reply & WANT_STRING) != 0 ? reply_str : NULL,
                                  sizeof (reply_str) - 1);
        if ((wait_reply & WANT_STRING) != 0 && !retry && level == 0 && code == 421)
        {
            retry = TRUE;
            level = 1;
            status = ftpfs_reconnect (me, super) ? 1 : 0;
            level = 0;
            if (status != 0 && (write (ftp_super->sock, cmdstr->str, cmdstr->len) > 0))
                goto ok;
        }
        retry = FALSE;
        g_string_free (cmdstr, TRUE);
        return status;
    }

    g_string_free (cmdstr, TRUE);
    return COMPLETE;
}

/* --------------------------------------------------------------------------------------------- */

static struct vfs_s_super *
ftpfs_new_archive (struct vfs_class *me)
{
    ftp_super_t *arch;

    arch = g_new0 (ftp_super_t, 1);
    arch->base.me = me;
    arch->base.name = g_strdup (PATH_SEP_STR);
    arch->sock = -1;
    arch->use_passive_connection = ftpfs_use_passive_connections;
    arch->strict = ftpfs_use_unix_list_options ? RFC_AUTODETECT : RFC_STRICT;
    arch->isbinary = TYPE_UNKNOWN;

    return VFS_SUPER (arch);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);

    if (ftp_super->sock != -1)
    {
        vfs_print_message (_("ftpfs: Disconnecting from %s"), super->path_element->host);
        ftpfs_command (me, super, NONE, "%s", "QUIT");
        close (ftp_super->sock);
    }
    g_free (ftp_super->current_dir);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_changetype (struct vfs_class *me, struct vfs_s_super *super, int binary)
{
    if (binary != FTP_SUPER (super)->isbinary)
    {
        if (ftpfs_command (me, super, WAIT_REPLY, "TYPE %c", binary ? 'I' : 'A') != COMPLETE)
            ERRNOR (EIO, -1);
        FTP_SUPER (super)->isbinary = binary;
    }
    return binary;
}

/* --------------------------------------------------------------------------------------------- */
/* This routine logs the user in */

static int
ftpfs_login_server (struct vfs_class *me, struct vfs_s_super *super, const char *netrcpass)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    char *pass;
    char *op;
    char *name;                 /* login user name */
    gboolean anon = FALSE;
    char reply_string[BUF_MEDIUM];

    ftp_super->isbinary = TYPE_UNKNOWN;

    if (super->path_element->password != NULL)  /* explicit password */
        op = g_strdup (super->path_element->password);
    else if (netrcpass != NULL) /* password from netrc */
        op = g_strdup (netrcpass);
    else if (strcmp (super->path_element->user, "anonymous") == 0
             || strcmp (super->path_element->user, "ftp") == 0)
    {
        if (ftpfs_anonymous_passwd == NULL)     /* default anonymous password */
            ftpfs_init_passwd ();
        op = g_strdup (ftpfs_anonymous_passwd);
        anon = TRUE;
    }
    else
    {                           /* ask user */
        char *p;

        p = g_strdup_printf (_("FTP: Password required for %s"), super->path_element->user);
        op = vfs_get_password (p);
        g_free (p);
        if (op == NULL)
            ERRNOR (EPERM, 0);
        super->path_element->password = g_strdup (op);
    }

    if (!anon || me->logfile != NULL)
        pass = op;
    else
    {
        pass = g_strconcat ("-", op, (char *) NULL);
        wipe_password (op);
    }

    /* Proxy server accepts: username@host-we-want-to-connect */
    if (ftp_super->proxy != NULL)
        name =
            g_strconcat (super->path_element->user, "@",
                         super->path_element->host[0] ==
                         '!' ? super->path_element->host + 1 : super->path_element->host,
                         (char *) NULL);
    else
        name = g_strdup (super->path_element->user);

    if (ftpfs_get_reply (me, ftp_super->sock, reply_string, sizeof (reply_string) - 1) == COMPLETE)
    {
        char *reply_up;

        reply_up = g_ascii_strup (reply_string, -1);
        ftp_super->remote_is_amiga = strstr (reply_up, "AMIGA") != NULL;
        if (strstr (reply_up, " SPFTP/1.0.0000 SERVER ") != NULL)       /* handles `LIST -la` in a weird way */
            ftp_super->strict = RFC_STRICT;
        g_free (reply_up);

        if (me->logfile != NULL)
        {
            fprintf (me->logfile, "MC -- remote_is_amiga = %s\n",
                     ftp_super->remote_is_amiga ? "yes" : "no");
            fflush (me->logfile);
        }

        vfs_print_message ("%s", _("ftpfs: sending login name"));

        switch (ftpfs_command (me, super, WAIT_REPLY, "USER %s", name))
        {
        case CONTINUE:
            vfs_print_message ("%s", _("ftpfs: sending user password"));
            code = ftpfs_command (me, super, WAIT_REPLY, "PASS %s", pass);
            if (code == CONTINUE)
            {
                char *p;

                p = g_strdup_printf (_("FTP: Account required for user %s"),
                                     super->path_element->user);
                op = input_dialog (p, _("Account:"), MC_HISTORY_FTPFS_ACCOUNT, "",
                                   INPUT_COMPLETE_USERNAMES);
                g_free (p);
                if (op == NULL)
                    ERRNOR (EPERM, 0);
                vfs_print_message ("%s", _("ftpfs: sending user account"));
                code = ftpfs_command (me, super, WAIT_REPLY, "ACCT %s", op);
                g_free (op);
            }
            if (code != COMPLETE)
                break;

            MC_FALLTHROUGH;

        case COMPLETE:
            vfs_print_message ("%s", _("ftpfs: logged in"));
            wipe_password (pass);
            g_free (name);
            return TRUE;

        default:
            ftp_super->failed_on_login = TRUE;
            wipe_password (super->path_element->password);
            super->path_element->password = NULL;
            goto login_fail;
        }
    }

    message (D_ERROR, MSG_ERROR, _("ftpfs: Login incorrect for user %s "),
             super->path_element->user);

  login_fail:
    wipe_password (pass);
    g_free (name);
    ERRNOR (EPERM, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_load_no_proxy_list (void)
{
    /* FixMe: shouldn't be hardcoded!!! */
    char *mc_file;

    mc_file = g_build_filename (mc_global.sysconfig_dir, "mc.no_proxy", (char *) NULL);
    if (exist_file (mc_file))
    {
        FILE *npf;

        npf = fopen (mc_file, "r");
        if (npf != NULL)
        {
            char s[BUF_LARGE];  /* provide for BUF_LARGE characters */

            while (fgets (s, sizeof (s), npf) != NULL)
            {
                char *p;

                p = strchr (s, '\n');
                if (p == NULL)  /* skip bogus entries */
                {
                    int c;

                    while ((c = fgetc (npf)) != EOF && c != '\n')
                        ;
                }
                else if (p != s)
                {
                    *p = '\0';
                    no_proxy = g_slist_prepend (no_proxy, g_strdup (s));
                }
            }

            fclose (npf);
        }
    }

    g_free (mc_file);
}

/* --------------------------------------------------------------------------------------------- */
/* Return TRUE if FTP proxy should be used for this host, FALSE otherwise */

static gboolean
ftpfs_check_proxy (const char *host)
{

    if (ftpfs_proxy_host == NULL || *ftpfs_proxy_host == '\0' || host == NULL || *host == '\0')
        return FALSE;           /* sanity check */

    if (*host == '!')
        return TRUE;

    if (!ftpfs_always_use_proxy)
        return FALSE;

    if (strchr (host, '.') == NULL)
        return FALSE;

    if (no_proxy == NULL)
    {
        GSList *npe;

        ftpfs_load_no_proxy_list ();

        for (npe = no_proxy; npe != NULL; npe = g_slist_next (npe))
        {
            const char *domain = (const char *) npe->data;

            if (domain[0] == '.')
            {
                size_t ld, lh;

                ld = strlen (domain);
                lh = strlen (host);

                while (ld != 0 && lh != 0 && host[lh - 1] == domain[ld - 1])
                {
                    ld--;
                    lh--;
                }

                if (ld == 0)
                    return FALSE;
            }
            else if (g_ascii_strcasecmp (host, domain) == 0)
                return FALSE;
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_get_proxy_host_and_port (const char *proxy, char **host, int *port)
{
    vfs_path_element_t *path_element;

    path_element = vfs_url_split (proxy, FTP_COMMAND_PORT, URL_USE_ANONYMOUS);
    *host = path_element->host;
    path_element->host = NULL;
    *port = path_element->port;
    vfs_path_element_free (path_element);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_socket (struct vfs_class *me, struct vfs_s_super *super)
{
    struct addrinfo hints, *res, *curr_res;
    int my_socket = 0;
    char *host = NULL;
    char port[8];
    int tmp_port = 0;
    int e;

    (void) me;

    if (super->path_element->host == NULL || *super->path_element->host == '\0')
    {
        vfs_print_message ("%s", _("ftpfs: Invalid host name."));
        me->verrno = EINVAL;
        return (-1);
    }

    /* Use a proxy host? */
    /* Hosts to connect to that start with a ! should use proxy */
    if (FTP_SUPER (super)->proxy != NULL)
        ftpfs_get_proxy_host_and_port (ftpfs_proxy_host, &host, &tmp_port);
    else
    {
        host = g_strdup (super->path_element->host);
        tmp_port = super->path_element->port;
    }

    g_snprintf (port, sizeof (port), "%hu", (unsigned short) tmp_port);

    tty_enable_interrupt_key ();        /* clear the interrupt flag */

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

#ifdef AI_ADDRCONFIG
    /* By default, only look up addresses using address types for
     * which a local interface is configured (i.e. no IPv6 if no IPv6
     * interfaces, likewise for IPv4 (see RFC 3493 for details). */
    hints.ai_flags = AI_ADDRCONFIG;
#endif

    e = getaddrinfo (host, port, &hints, &res);

#ifdef AI_ADDRCONFIG
    if (e == EAI_BADFLAGS)
    {
        /* Retry with no flags if AI_ADDRCONFIG was rejected. */
        hints.ai_flags = 0;
        e = getaddrinfo (host, port, &hints, &res);
    }
#endif

    *port = '\0';

    if (e != 0)
    {
        tty_disable_interrupt_key ();
        vfs_print_message (_("ftpfs: %s"), gai_strerror (e));
        g_free (host);
        me->verrno = EINVAL;
        return (-1);
    }

    for (curr_res = res; curr_res != NULL; curr_res = curr_res->ai_next)
    {
        my_socket = socket (curr_res->ai_family, curr_res->ai_socktype, curr_res->ai_protocol);

        if (my_socket < 0)
        {
            if (curr_res->ai_next != NULL)
                continue;

            tty_disable_interrupt_key ();
            vfs_print_message (_("ftpfs: %s"), unix_error_string (errno));
            g_free (host);
            freeaddrinfo (res);
            me->verrno = errno;
            return (-1);
        }

        vfs_print_message (_("ftpfs: making connection to %s"), host);
        MC_PTR_FREE (host);

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        me->verrno = errno;
        close (my_socket);

        if (errno == EINTR && tty_got_interrupt ())
            vfs_print_message ("%s", _("ftpfs: connection interrupted by user"));
        else if (res->ai_next == NULL)
            vfs_print_message (_("ftpfs: connection to server failed: %s"),
                               unix_error_string (errno));
        else
            continue;

        freeaddrinfo (res);
        tty_disable_interrupt_key ();
        return (-1);
    }

    freeaddrinfo (res);
    tty_disable_interrupt_key ();
    return my_socket;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_archive_int (struct vfs_class *me, struct vfs_s_super *super)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int retry_seconds = 0;

    /* We do not want to use the passive if we are using proxies */
    if (ftp_super->proxy != NULL)
        ftp_super->use_passive_connection = ftpfs_use_passive_connections_over_proxy;

    do
    {
        ftp_super->failed_on_login = FALSE;

        ftp_super->sock = ftpfs_open_socket (me, super);
        if (ftp_super->sock == -1)
            return (-1);

        if (ftpfs_login_server (me, super, NULL))
        {
            /* Logged in, no need to retry the connection */
            break;
        }

        if (!ftp_super->failed_on_login)
            return (-1);

        /* Close only the socket descriptor */
        close (ftp_super->sock);

        if (ftpfs_retry_seconds != 0)
        {
            int count_down;

            retry_seconds = ftpfs_retry_seconds;
            tty_enable_interrupt_key ();
            for (count_down = retry_seconds; count_down != 0; count_down--)
            {
                vfs_print_message (_("Waiting to retry... %d (Control-G to cancel)"), count_down);
                sleep (1);
                if (tty_got_interrupt ())
                {
                    /* me->verrno = E; */
                    tty_disable_interrupt_key ();
                    return 0;
                }
            }
            tty_disable_interrupt_key ();
        }
    }
    while (retry_seconds != 0);

    ftp_super->current_dir = ftpfs_get_current_directory (me, super);
    if (ftp_super->current_dir == NULL)
        ftp_super->current_dir = g_strdup (PATH_SEP_STR);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_archive (struct vfs_s_super *super,
                    const vfs_path_t *vpath, const vfs_path_element_t *vpath_element)
{
    (void) vpath;

    super->path_element = ftpfs_correct_url_parameters (vpath_element);
    if (ftpfs_check_proxy (super->path_element->host))
        FTP_SUPER (super)->proxy = ftpfs_proxy_host;
    super->root =
        vfs_s_new_inode (vpath_element->class, super, ftpfs_default_stat (vpath_element->class));

    return ftpfs_open_archive_int (vpath_element->class, super);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_archive_same (const vfs_path_element_t *vpath_element, struct vfs_s_super *super,
                    const vfs_path_t *vpath, void *cookie)
{
    vfs_path_element_t *path_element;
    int result;

    (void) vpath;
    (void) cookie;

    path_element = ftpfs_correct_url_parameters (vpath_element);

    result = ((strcmp (path_element->host, super->path_element->host) == 0)
              && (strcmp (path_element->user, super->path_element->user) == 0)
              && (path_element->port == super->path_element->port)) ? 1 : 0;

    vfs_path_element_free (path_element);
    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* The returned directory should always contain a trailing slash */

static char *
ftpfs_get_current_directory (struct vfs_class *me, struct vfs_s_super *super)
{
    char buf[MC_MAXPATHLEN + 1];

    if (ftpfs_command (me, super, NONE, "%s", "PWD") == COMPLETE &&
        ftpfs_get_reply (me, FTP_SUPER (super)->sock, buf, sizeof (buf)) == COMPLETE)
    {
        char *bufp = NULL;
        char *bufq;

        for (bufq = buf; *bufq != '\0'; bufq++)
            if (*bufq == '"')
            {
                if (bufp == NULL)
                    bufp = bufq + 1;
                else
                {
                    *bufq = '\0';

                    if (*bufp != '\0')
                    {
                        if (!IS_PATH_SEP (bufq[-1]))
                        {
                            *bufq++ = PATH_SEP;
                            *bufq = '\0';
                        }

                        if (IS_PATH_SEP (*bufp))
                            return g_strdup (bufp);

                        /* If the remote server is an Amiga a leading slash
                           might be missing. MC needs it because it is used
                           as separator between hostname and path internally. */
                        return g_strconcat (PATH_SEP_STR, bufp, (char *) NULL);
                    }

                    break;
                }
            }
    }

    me->verrno = EIO;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive PASV FTP connection */

static gboolean
ftpfs_setup_passive_pasv (struct vfs_class *me, struct vfs_s_super *super,
                          int my_socket, struct sockaddr_storage *sa, socklen_t *salen)
{
    char *c;
    char n[6];
    int xa, xb, xc, xd, xe, xf;

    if (ftpfs_command (me, super, WAIT_REPLY | WANT_STRING, "%s", "PASV") != COMPLETE)
        return FALSE;

    /* Parse remote parameters */
    for (c = reply_str + 4; *c != '\0' && !isdigit ((unsigned char) *c); c++)
        ;

    if (*c == '\0' || !isdigit ((unsigned char) *c))
        return FALSE;

    /* cppcheck-suppress invalidscanf */
    if (sscanf (c, "%d,%d,%d,%d,%d,%d", &xa, &xb, &xc, &xd, &xe, &xf) != 6)
        return FALSE;

    n[0] = (unsigned char) xa;
    n[1] = (unsigned char) xb;
    n[2] = (unsigned char) xc;
    n[3] = (unsigned char) xd;
    n[4] = (unsigned char) xe;
    n[5] = (unsigned char) xf;

    memcpy (&(((struct sockaddr_in *) sa)->sin_addr.s_addr), (void *) n, 4);
    memcpy (&(((struct sockaddr_in *) sa)->sin_port), (void *) &n[4], 2);

    return (connect (my_socket, (struct sockaddr *) sa, *salen) >= 0);
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive EPSV FTP connection */

static gboolean
ftpfs_setup_passive_epsv (struct vfs_class *me, struct vfs_s_super *super,
                          int my_socket, struct sockaddr_storage *sa, socklen_t *salen)
{
    char *c;
    int port;

    if (ftpfs_command (me, super, WAIT_REPLY | WANT_STRING, "%s", "EPSV") != COMPLETE)
        return FALSE;

    /* (|||<port>|) */
    c = strchr (reply_str, '|');
    if (c == NULL || strlen (c) <= 3)
        return FALSE;

    c += 3;
    port = atoi (c);
    if (port < 0 || port > 65535)
        return FALSE;

    port = htons (port);

    switch (sa->ss_family)
    {
    case AF_INET:
        ((struct sockaddr_in *) sa)->sin_port = port;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *) sa)->sin6_port = port;
        break;
    default:
        break;
    }

    return (connect (my_socket, (struct sockaddr *) sa, *salen) >= 0);
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive ftp connection, we use it for source routed connections */

static gboolean
ftpfs_setup_passive (struct vfs_class *me, struct vfs_s_super *super,
                     int my_socket, struct sockaddr_storage *sa, socklen_t *salen)
{
    /* It's IPV4, so try PASV first, some servers and ALGs get confused by EPSV */
    if (sa->ss_family == AF_INET)
    {
        if (!ftpfs_setup_passive_pasv (me, super, my_socket, sa, salen))
            /* An IPV4 FTP server might support EPSV, so if PASV fails we can try EPSV anyway */
            if (!ftpfs_setup_passive_epsv (me, super, my_socket, sa, salen))
                return FALSE;
    }
    /* It's IPV6, so EPSV is our only hope */
    else if (!ftpfs_setup_passive_epsv (me, super, my_socket, sa, salen))
        return FALSE;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Active PORT or EPRT FTP connection */

static int
ftpfs_setup_active (struct vfs_class *me, struct vfs_s_super *super,
                    struct sockaddr_storage data_addr, socklen_t data_addrlen)
{
    unsigned short int port;
    char *addr;
    unsigned int af;
    int res;

    switch (data_addr.ss_family)
    {
    case AF_INET:
        af = FTP_INET;
        port = ((struct sockaddr_in *) &data_addr)->sin_port;
        break;
    case AF_INET6:
        af = FTP_INET6;
        port = ((struct sockaddr_in6 *) &data_addr)->sin6_port;
        break;
    default:
        /* Not implemented */
        return 0;
    }

    addr = g_try_malloc (NI_MAXHOST);
    if (addr == NULL)
        ERRNOR (ENOMEM, -1);

    res =
        getnameinfo ((struct sockaddr *) &data_addr, data_addrlen, addr, NI_MAXHOST, NULL, 0,
                     NI_NUMERICHOST);
    if (res != 0)
    {
        const char *err_str;

        g_free (addr);

        if (res == EAI_SYSTEM)
        {
            me->verrno = errno;
            err_str = unix_error_string (me->verrno);
        }
        else
        {
            me->verrno = EIO;
            err_str = gai_strerror (res);
        }

        vfs_print_message (_("ftpfs: could not make address-to-name translation: %s"), err_str);

        return (-1);
    }

    /* If we are talking to an IPV4 server, try PORT, and, only if it fails, go for EPRT */
    if (af == FTP_INET)
    {
        unsigned char *a = (unsigned char *) &((struct sockaddr_in *) &data_addr)->sin_addr;
        unsigned char *p = (unsigned char *) &port;

        if (ftpfs_command (me, super, WAIT_REPLY,
                           "PORT %u,%u,%u,%u,%u,%u", a[0], a[1], a[2], a[3],
                           p[0], p[1]) == COMPLETE)
        {
            g_free (addr);
            return 1;
        }
    }

    /*
     * Converts network MSB first order to host byte order (LSB
     * first on i386). If we do it earlier, we will run into an
     * endianness issue, because the server actually expects to see
     * "PORT A,D,D,R,MSB,LSB" in the PORT command.
     */
    port = ntohs (port);

    /* We are talking to an IPV6 server or PORT failed, so we can try EPRT anyway */
    res =
        (ftpfs_command (me, super, WAIT_REPLY, "EPRT |%u|%s|%hu|", af, addr, port) ==
         COMPLETE) ? 1 : 0;
    g_free (addr);
    return res;
}

/* --------------------------------------------------------------------------------------------- */
/* Initialize a socket for FTP DATA connection */

static int
ftpfs_init_data_socket (struct vfs_class *me, struct vfs_s_super *super,
                        struct sockaddr_storage *data_addr, socklen_t *data_addrlen)
{
    const unsigned int attempts = 10;
    unsigned int i;
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int result;

    for (i = 0; i < attempts; i++)
    {
        memset (data_addr, 0, sizeof (*data_addr));
        *data_addrlen = sizeof (*data_addr);

        if (ftp_super->use_passive_connection)
        {
            result = getpeername (ftp_super->sock, (struct sockaddr *) data_addr, data_addrlen);
            if (result == 0)
                break;

            me->verrno = errno;

            if (me->verrno == ENOTCONN)
            {
                vfs_print_message (_("ftpfs: try reconnect to server, attempt %u"), i);
                if (ftpfs_reconnect (me, super))
                    continue;   /* get name of new socket */
            }
            else
            {
                /* error -- stop loop */
                vfs_print_message (_("ftpfs: could not get socket name: %s"),
                                   unix_error_string (me->verrno));
            }
        }
        else
        {
            result = getsockname (ftp_super->sock, (struct sockaddr *) data_addr, data_addrlen);
            if (result == 0)
                break;

            me->verrno = errno;

            vfs_print_message (_("ftpfs: try reconnect to server, attempt %u"), i);
            if (ftpfs_reconnect (me, super))
                continue;       /* get name of new socket */

            /* error -- stop loop */
            vfs_print_message ("%s", _("ftpfs: could not reconnect to server"));
        }

        i = attempts;
    }

    if (i >= attempts)
        return (-1);

    switch (data_addr->ss_family)
    {
    case AF_INET:
        ((struct sockaddr_in *) data_addr)->sin_port = 0;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *) data_addr)->sin6_port = 0;
        break;
    default:
        vfs_print_message ("%s", _("ftpfs: invalid address family"));
        ERRNOR (EINVAL, -1);
    }

    result = socket (data_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (result < 0)
    {
        me->verrno = errno;
        vfs_print_message (_("ftpfs: could not create socket: %s"), unix_error_string (me->verrno));
        result = -1;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* Initialize FTP DATA connection */

static int
ftpfs_initconn (struct vfs_class *me, struct vfs_s_super *super)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    struct sockaddr_storage data_addr;
    socklen_t data_addrlen;

    /*
     * Don't factor socket initialization out of these conditionals,
     * because ftpfs_init_data_socket initializes it in different way
     * depending on use_passive_connection flag.
     */

    /* Try to establish a passive connection first (if requested) */
    if (ftp_super->use_passive_connection)
    {
        int data_sock;

        data_sock = ftpfs_init_data_socket (me, super, &data_addr, &data_addrlen);
        if (data_sock < 0)
            return (-1);

        if (ftpfs_setup_passive (me, super, data_sock, &data_addr, &data_addrlen))
            return data_sock;

        vfs_print_message ("%s", _("ftpfs: could not setup passive mode"));
        ftp_super->use_passive_connection = FALSE;

        close (data_sock);
    }

    /* If passive setup is disabled or failed, fallback to active connections */
    if (!ftp_super->use_passive_connection)
    {
        int data_sock;

        data_sock = ftpfs_init_data_socket (me, super, &data_addr, &data_addrlen);
        if (data_sock < 0)
            return (-1);

        if ((bind (data_sock, (struct sockaddr *) &data_addr, data_addrlen) != 0) ||
            (getsockname (data_sock, (struct sockaddr *) &data_addr, &data_addrlen) != 0) ||
            (listen (data_sock, 1) != 0))
        {
            close (data_sock);
            ERRNOR (errno, -1);
        }

        if (ftpfs_setup_active (me, super, data_addr, data_addrlen) != 0)
            return data_sock;

        close (data_sock);
    }

    /* Restore the initial value of use_passive_connection (for subsequent retries) */
    ftp_super->use_passive_connection =
        ftp_super->proxy !=
        NULL ? ftpfs_use_passive_connections_over_proxy : ftpfs_use_passive_connections;

    me->verrno = EIO;
    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_data_connection (struct vfs_class *me, struct vfs_s_super *super, const char *cmd,
                            const char *remote, int isbinary, int reget)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int s, j, data;

    /* FTP doesn't allow to open more than one file at a time */
    if (ftp_super->ctl_connection_busy)
        return (-1);

    s = ftpfs_initconn (me, super);
    if (s == -1)
        return (-1);

    if (ftpfs_changetype (me, super, isbinary) == -1)
    {
        close (s);
        return (-1);
    }

    if (reget > 0)
    {
        j = ftpfs_command (me, super, WAIT_REPLY, "REST %d", reget);
        if (j != CONTINUE)
        {
            close (s);
            ERRNOR (EIO, -1);
        }
    }

    if (remote == NULL)
        j = ftpfs_command (me, super, WAIT_REPLY, "%s", cmd);
    else
    {
        char *remote_path;

        remote_path = ftpfs_translate_path (me, super, remote);
        j = ftpfs_command (me, super, WAIT_REPLY, "%s /%s", cmd,
                           /* WarFtpD can't STORE //filename */
                           IS_PATH_SEP (*remote_path) ? remote_path + 1 : remote_path);
        g_free (remote_path);
    }

    if (j != PRELIM)
    {
        close (s);
        ERRNOR (EPERM, -1);
    }

    if (ftp_super->use_passive_connection)
        data = s;
    else
    {
        struct sockaddr_storage from;
        socklen_t fromlen = sizeof (from);

        tty_enable_interrupt_key ();
        data = accept (s, (struct sockaddr *) &from, &fromlen);
        if (data < 0)
            me->verrno = errno;
        tty_disable_interrupt_key ();
        close (s);
        if (data < 0)
            return (-1);
    }

    ftp_super->ctl_connection_busy = TRUE;
    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_linear_abort (struct vfs_class *me, vfs_file_handler_t *fh)
{
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    ftp_super_t *ftp_super = FTP_SUPER (super);
    static unsigned char const ipbuf[3] = { IAC, IP, IAC };
    fd_set mask;
    int dsock = FH_SOCK;

    FH_SOCK = -1;
    ftp_super->ctl_connection_busy = FALSE;

    vfs_print_message ("%s", _("ftpfs: aborting transfer."));

    if (send (ftp_super->sock, ipbuf, sizeof (ipbuf), MSG_OOB) != sizeof (ipbuf))
    {
        vfs_print_message (_("ftpfs: abort error: %s"), unix_error_string (errno));
        if (dsock != -1)
            close (dsock);
        return;
    }

    if (ftpfs_command (me, super, NONE, "%cABOR", DM) != COMPLETE)
    {
        vfs_print_message ("%s", _("ftpfs: abort failed"));
        if (dsock != -1)
            close (dsock);
        return;
    }

    if (dsock != -1)
    {
        FD_ZERO (&mask);
        FD_SET (dsock, &mask);

        if (select (dsock + 1, &mask, NULL, NULL, NULL) > 0)
        {
            gint64 start_tim;
            char buf[BUF_8K];

            start_tim = g_get_monotonic_time ();

            /* flush the remaining data */
            while (read (dsock, buf, sizeof (buf)) > 0)
            {
                gint64 tim;

                tim = g_get_monotonic_time ();

                if (tim > start_tim + ABORT_TIMEOUT)
                {
                    /* server keeps sending, drop the connection and ftpfs_reconnect */
                    close (dsock);
                    ftpfs_reconnect (me, super);
                    return;
                }
            }
        }
        close (dsock);
    }

    if ((ftpfs_get_reply (me, ftp_super->sock, NULL, 0) == TRANSIENT) && (code == 426))
        ftpfs_get_reply (me, ftp_super->sock, NULL, 0);
}

/* --------------------------------------------------------------------------------------------- */

#if 0
static void
resolve_symlink_without_ls_options (struct vfs_class *me, struct vfs_s_super *super,
                                    struct vfs_s_inode *dir)
{
    struct linklist *flist;
    struct direntry *fe, *fel;
    char tmp[MC_MAXPATHLEN];

    dir->symlink_status = FTPFS_RESOLVING_SYMLINKS;
    for (flist = dir->file_list->next; flist != dir->file_list; flist = flist->next)
    {
        /* flist->data->l_stat is already initialized with 0 */
        fel = flist->data;
        if (S_ISLNK (fel->s.st_mode) && fel->linkname != NULL)
        {
            int depth;

            if (IS_PATH_SEP (fel->linkname[0]))
            {
                if (strlen (fel->linkname) >= MC_MAXPATHLEN)
                    continue;
                strcpy (tmp, fel->linkname);
            }
            else
            {
                if ((strlen (dir->remote_path) + strlen (fel->linkname)) >= MC_MAXPATHLEN)
                    continue;
                strcpy (tmp, dir->remote_path);
                if (tmp[1] != '\0')
                    strcat (tmp, PATH_SEP_STR);
                strcat (tmp + 1, fel->linkname);
            }

            for (depth = 0; depth < 100; depth++)
            {                   /* depth protects against recursive symbolic links */
                canonicalize_pathname (tmp);
                fe = _get_file_entry_t (bucket, tmp, 0, 0);
                if (fe != NULL)
                {
                    if (S_ISLNK (fe->s.st_mode) && fe->l_stat == 0)
                    {
                        /* Symlink points to link which isn't resolved, yet. */
                        if (IS_PATH_SEP (fe->linkname[0]))
                        {
                            if (strlen (fe->linkname) >= MC_MAXPATHLEN)
                                break;
                            strcpy (tmp, fe->linkname);
                        }
                        else
                        {
                            /* at this point tmp looks always like this
                               /directory/filename, i.e. no need to check
                               strrchr's return value */
                            *(strrchr (tmp, PATH_SEP) + 1) = '\0';      /* dirname */
                            if ((strlen (tmp) + strlen (fe->linkname)) >= MC_MAXPATHLEN)
                                break;
                            strcat (tmp, fe->linkname);
                        }
                        continue;
                    }
                    else
                    {
                        fel->l_stat = g_new (struct stat, 1);
                        if (S_ISLNK (fe->s.st_mode))
                            *fel->l_stat = *fe->l_stat;
                        else
                            *fel->l_stat = fe->s;
                        (*fel->l_stat).st_ino = bucket->__inode_counter++;
                    }
                }
                break;
            }
        }
    }

    dir->symlink_status = FTPFS_RESOLVED_SYMLINKS;
}

/* --------------------------------------------------------------------------------------------- */

static void
resolve_symlink_with_ls_options (struct vfs_class *me, struct vfs_s_super *super,
                                 struct vfs_s_inode *dir)
{
    char buffer[2048] = "", *filename;
    int sock;
    FILE *fp;
    struct stat s;
    struct linklist *flist;
    struct direntry *fe;
    int switch_method = 0;

    dir->symlink_status = FTPFS_RESOLVED_SYMLINKS;
    if (strchr (dir->remote_path, ' ') == NULL)
        sock = ftpfs_open_data_connection (bucket, "LIST -lLa", dir->remote_path, TYPE_ASCII, 0);
    else
    {
        if (ftpfs_chdir_internal (bucket, dir->remote_path) != COMPLETE)
        {
            vfs_print_message ("%s", _("ftpfs: CWD failed."));
            return;
        }

        sock = ftpfs_open_data_connection (bucket, "LIST -lLa", ".", TYPE_ASCII, 0);
    }

    if (sock == -1)
    {
        vfs_print_message ("%s", _("ftpfs: couldn't resolve symlink"));
        return;
    }

    fp = fdopen (sock, "r");
    if (fp == NULL)
    {
        close (sock);
        vfs_print_message ("%s", _("ftpfs: couldn't resolve symlink"));
        return;
    }
    tty_enable_interrupt_key ();
    flist = dir->file_list->next;

    while (TRUE)
    {
        do
        {
            if (flist == dir->file_list)
                goto done;

            fe = flist->data;
            flist = flist->next;
        }
        while (!S_ISLNK (fe->s.st_mode));

        while (TRUE)
        {
            if (fgets (buffer, sizeof (buffer), fp) == NULL)
                goto done;

            if (me->logfile != NULL)
            {
                fputs (buffer, me->logfile);
                fflush (me->logfile);
            }

            vfs_die ("This code should be commented out\n");

            if (vfs_parse_ls_lga (buffer, &s, &filename, NULL))
            {
                int r;

                r = strcmp (fe->name, filename);
                g_free (filename);
                if (r == 0)
                {
                    if (S_ISLNK (s.st_mode))
                    {
                        /* This server doesn't understand LIST -lLa */
                        switch_method = 1;
                        goto done;
                    }

                    fe->l_stat = g_try_new (struct stat, 1);
                    if (fe->l_stat == NULL)
                        goto done;

                    *fe->l_stat = s;
                    (*fe->l_stat).st_ino = bucket->__inode_counter++;
                    break;
                }

                if (r < 0)
                    break;
            }
        }
    }

  done:
    while (fgets (buffer, sizeof (buffer), fp) != NULL)
        ;
    tty_disable_interrupt_key ();
    fclose (fp);
    ftpfs_get_reply (me, FTP_SUPER (super)->sock, NULL, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
resolve_symlink (struct vfs_class *me, struct vfs_s_super *super, struct vfs_s_inode *dir)
{
    vfs_print_message ("%s", _("Resolving symlink..."));

    if (FTP_SUPER (super)->strict_rfc959_list_cmd)
        resolve_symlink_without_ls_options (me, super, dir);
    else
        resolve_symlink_with_ls_options (me, super, dir);
}
#endif

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, const char *remote_path)
{
    struct vfs_s_super *super = dir->super;
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int sock;
    char lc_buffer[BUF_8K];
    int res;
    gboolean cd_first;
    GSList *dirlist = NULL;
    GSList *entlist;
    GSList *iter;
    int err_count = 0;

    cd_first = ftpfs_first_cd_then_ls || (ftp_super->strict == RFC_STRICT)
        || (strchr (remote_path, ' ') != NULL);

  again:
    vfs_print_message (_("ftpfs: Reading FTP directory %s... %s%s"),
                       remote_path,
                       ftp_super->strict ==
                       RFC_STRICT ? _("(strict rfc959)") : "", cd_first ? _("(chdir first)") : "");

    if (cd_first && ftpfs_chdir_internal (me, super, remote_path) != COMPLETE)
    {
        me->verrno = ENOENT;
        vfs_print_message ("%s", _("ftpfs: CWD failed."));
        return (-1);
    }

    dir->timestamp = g_get_monotonic_time () + ftpfs_directory_timeout * G_USEC_PER_SEC;

    if (ftp_super->strict == RFC_STRICT)
        sock = ftpfs_open_data_connection (me, super, "LIST", 0, TYPE_ASCII, 0);
    else if (cd_first)
        /* Dirty hack to avoid autoprepending / to . */
        /* Wu-ftpd produces strange output for '/' if 'LIST -la .' used */
        sock = ftpfs_open_data_connection (me, super, "LIST -la", 0, TYPE_ASCII, 0);
    else
    {
        char *path;

        /* Trailing "/." is necessary if remote_path is a symlink */
        path = g_strconcat (remote_path, PATH_SEP_STR ".", (char *) NULL);
        sock = ftpfs_open_data_connection (me, super, "LIST -la", path, TYPE_ASCII, 0);
        g_free (path);
    }

    if (sock == -1)
    {
      fallback:
        if (ftp_super->strict == RFC_AUTODETECT)
        {
            /* It's our first attempt to get a directory listing from this
               server (UNIX style LIST command) */
            ftp_super->strict = RFC_STRICT;
            /* I hate goto, but recursive call needs another 8K on stack */
            /* return ftpfs_dir_load (me, dir, remote_path); */
            cd_first = TRUE;
            goto again;
        }

        vfs_print_message ("%s", _("ftpfs: failed; nowhere to fallback to"));
        ERRNOR (EACCES, -1);
    }

    /* read full directory list, then parse it */
    while ((res = vfs_s_get_line_interruptible (me, lc_buffer, sizeof (lc_buffer), sock)) != 0)
    {
        if (res == EINTR)
        {
            me->verrno = ECONNRESET;
            close (sock);
            ftp_super->ctl_connection_busy = FALSE;
            ftpfs_get_reply (me, ftp_super->sock, NULL, 0);
            g_slist_free_full (dirlist, g_free);
            vfs_print_message (_("%s: failure"), me->name);
            return (-1);
        }

        if (me->logfile != NULL)
        {
            fputs (lc_buffer, me->logfile);
            fputs ("\n", me->logfile);
            fflush (me->logfile);
        }

        dirlist = g_slist_prepend (dirlist, g_strdup (lc_buffer));
    }

    close (sock);
    ftp_super->ctl_connection_busy = FALSE;
    me->verrno = E_REMOTE;
    if ((ftpfs_get_reply (me, ftp_super->sock, NULL, 0) != COMPLETE))
    {
        g_slist_free_full (dirlist, g_free);
        goto fallback;
    }

    if (dirlist == NULL && !cd_first)
    {
        /* The LIST command may produce an empty output. In such scenario
           it is not clear whether this is caused by  'remote_path' being
           a non-existent path or for some other reason (listing empty
           directory without the -a option, non-readable directory, etc.).

           Since 'dir_load' is a crucial method, when it comes to determine
           whether a given path is a _directory_, the code must try its best
           to determine the type of 'remote_path'. The only reliable way to
           achieve this is through issuing a CWD command. */

        cd_first = TRUE;
        goto again;
    }

    /* parse server's reply */
    dirlist = g_slist_reverse (dirlist);        /* restore order */
    entlist = ftpfs_parse_long_list (me, dir, dirlist, &err_count);
    g_slist_free_full (dirlist, g_free);

    for (iter = entlist; iter != NULL; iter = g_slist_next (iter))
        vfs_s_insert_entry (me, dir, VFS_ENTRY (iter->data));

    g_slist_free (entlist);

    if (ftp_super->strict == RFC_AUTODETECT)
        ftp_super->strict = RFC_DARING;

    vfs_print_message (_("%s: done."), me->name);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_file_store (struct vfs_class *me, vfs_file_handler_t *fh, char *name, char *localname)
{
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);
    ftp_super_t *ftp_super = FTP_SUPER (super);
    ftp_file_handler_t *ftp = FTP_FILE_HANDLER (fh);

    int h, sock;
    off_t n_stored = 0;
#ifdef HAVE_STRUCT_LINGER_L_LINGER
    struct linger li;
#else
    int flag_one = 1;
#endif
    char lc_buffer[BUF_8K];
    struct stat s;
    char *w_buf;

    h = open (localname, O_RDONLY);
    if (h == -1)
        ERRNOR (EIO, -1);

    if (fstat (h, &s) == -1)
    {
        me->verrno = errno;
        close (h);
        return (-1);
    }

    sock =
        ftpfs_open_data_connection (me, super, ftp->append ? "APPE" : "STOR", name, TYPE_BINARY, 0);
    if (sock < 0)
    {
        close (h);
        return (-1);
    }
#ifdef HAVE_STRUCT_LINGER_L_LINGER
    li.l_onoff = 1;
    li.l_linger = 120;
    setsockopt (sock, SOL_SOCKET, SO_LINGER, (char *) &li, sizeof (li));
#else
    setsockopt (sock, SOL_SOCKET, SO_LINGER, &flag_one, sizeof (flag_one));
#endif

    tty_enable_interrupt_key ();
    while (TRUE)
    {
        ssize_t n_read, n_written;

        while ((n_read = read (h, lc_buffer, sizeof (lc_buffer))) == -1)
        {
            if (errno != EINTR)
            {
                me->verrno = errno;
                goto error_return;
            }
            if (tty_got_interrupt ())
            {
                me->verrno = EINTR;
                goto error_return;
            }
        }
        if (n_read == 0)
            break;

        n_stored += n_read;
        w_buf = lc_buffer;

        while ((n_written = write (sock, w_buf, n_read)) != n_read)
        {
            if (n_written == -1)
            {
                if (errno == EINTR && !tty_got_interrupt ())
                    continue;

                me->verrno = errno;
                goto error_return;
            }

            w_buf += n_written;
            n_read -= n_written;
        }

        vfs_print_message ("%s: %" PRIuMAX "/%" PRIuMAX,
                           _("ftpfs: storing file"), (uintmax_t) n_stored, (uintmax_t) s.st_size);
    }
    tty_disable_interrupt_key ();

    close (sock);
    ftp_super->ctl_connection_busy = FALSE;
    close (h);

    if (ftpfs_get_reply (me, ftp_super->sock, NULL, 0) != COMPLETE)
        ERRNOR (EIO, -1);
    return 0;

  error_return:
    tty_disable_interrupt_key ();
    close (sock);
    ftp_super->ctl_connection_busy = FALSE;
    close (h);

    ftpfs_get_reply (me, ftp_super->sock, NULL, 0);
    return (-1);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_linear_start (struct vfs_class *me, vfs_file_handler_t *fh, off_t offset)
{
    char *name;

    name = vfs_s_fullpath (me, fh->ino);
    if (name == NULL)
        return 0;

    FH_SOCK =
        ftpfs_open_data_connection (me, VFS_FILE_HANDLER_SUPER (fh), "RETR", name, TYPE_BINARY,
                                    offset);
    g_free (name);
    if (FH_SOCK == -1)
        ERRNOR (EACCES, 0);

    fh->linear = LS_LINEAR_OPEN;
    FTP_FILE_HANDLER (fh)->append = FALSE;
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
ftpfs_linear_read (struct vfs_class *me, vfs_file_handler_t *fh, void *buf, size_t len)
{
    ssize_t n;
    struct vfs_s_super *super = VFS_FILE_HANDLER_SUPER (fh);

    while ((n = read (FH_SOCK, buf, len)) < 0)
    {
        if ((errno == EINTR) && !tty_got_interrupt ())
            continue;
        break;
    }

    if (n < 0)
        ftpfs_linear_abort (me, fh);
    else if (n == 0)
    {
        FTP_SUPER (super)->ctl_connection_busy = FALSE;
        close (FH_SOCK);
        FH_SOCK = -1;
        if ((ftpfs_get_reply (me, FTP_SUPER (super)->sock, NULL, 0) != COMPLETE))
            ERRNOR (E_REMOTE, -1);
        return 0;
    }

    ERRNOR (errno, n);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_linear_close (struct vfs_class *me, vfs_file_handler_t *fh)
{
    if (FH_SOCK != -1)
        ftpfs_linear_abort (me, fh);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_ctl (void *fh, int ctlop, void *arg)
{
    (void) arg;

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

            v = vfs_s_select_on_two (FH_SOCK, 0);
            return (((v < 0) && (errno == EINTR)) || v == 0) ? 1 : 0;
        }
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_send_command (const vfs_path_t *vpath, const char *cmd, int flags)
{
    const char *rpath;
    char *p;
    struct vfs_s_super *super;
    int r;
    struct vfs_class *me;
    gboolean flush_directory_cache = (flags & OPT_FLUSH) != 0;

    me = VFS_CLASS (vfs_path_get_last_path_vfs (vpath));

    rpath = vfs_s_get_path (vpath, &super, 0);
    if (rpath == NULL)
        return (-1);

    p = ftpfs_translate_path (me, super, rpath);
    r = ftpfs_command (me, super, WAIT_REPLY, cmd, p);
    g_free (p);
    vfs_stamp_create (vfs_ftpfs_ops, super);
    if ((flags & OPT_IGNORE_ERROR) != 0)
        r = COMPLETE;
    if (r != COMPLETE)
    {
        me->verrno = EPERM;
        return (-1);
    }
    if (flush_directory_cache)
        vfs_s_invalidate (me, super);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_stat (const vfs_path_t *vpath, struct stat *buf)
{
    int ret;

    ret = vfs_s_stat (vpath, buf);
    ftpfs_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_lstat (const vfs_path_t *vpath, struct stat *buf)
{
    int ret;

    ret = vfs_s_lstat (vpath, buf);
    ftpfs_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_fstat (void *vfs_info, struct stat *buf)
{
    int ret;

    ret = vfs_s_fstat (vfs_info, buf);
    ftpfs_set_blksize (buf);
    return ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chmod (const vfs_path_t *vpath, mode_t mode)
{
    char buf[BUF_SMALL];
    int ret;

    g_snprintf (buf, sizeof (buf), "SITE CHMOD %4.4o /%%s", (unsigned int) (mode & 07777));
    ret = ftpfs_send_command (vpath, buf, OPT_FLUSH);
    return ftpfs_ignore_chattr_errors ? 0 : ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chown (const vfs_path_t *vpath, uid_t owner, gid_t group)
{
#if 0
    (void) vpath;
    (void) owner;
    (void) group;

    me->verrno = EPERM;
    return (-1);
#else
    /* Everyone knows it is not possible to chown remotely, so why bother them.
       If someone's root, then copy/move will always try to chown it... */
    (void) vpath;
    (void) owner;
    (void) group;
    return 0;
#endif
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_unlink (const vfs_path_t *vpath)
{
    return ftpfs_send_command (vpath, "DELE /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

/* Return TRUE if path is the same directory as the one we are in now */
static gboolean
ftpfs_is_same_dir (struct vfs_class *me, struct vfs_s_super *super, const char *path)
{
    (void) me;

    return (FTP_SUPER (super)->current_dir != NULL
            && strcmp (path, FTP_SUPER (super)->current_dir) == 0);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chdir_internal (struct vfs_class *me, struct vfs_s_super *super, const char *remote_path)
{
    ftp_super_t *ftp_super = FTP_SUPER (super);
    int r;
    char *p;

    if (!ftp_super->cwd_deferred && ftpfs_is_same_dir (me, super, remote_path))
        return COMPLETE;

    p = ftpfs_translate_path (me, super, remote_path);
    r = ftpfs_command (me, super, WAIT_REPLY, "CWD /%s", p);
    g_free (p);

    if (r != COMPLETE)
        me->verrno = EIO;
    else
    {
        g_free (ftp_super->current_dir);
        ftp_super->current_dir = g_strdup (remote_path);
        ftp_super->cwd_deferred = FALSE;
    }
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_rename (const vfs_path_t *vpath1, const vfs_path_t *vpath2)
{
    ftpfs_send_command (vpath1, "RNFR /%s", OPT_FLUSH);
    return ftpfs_send_command (vpath2, "RNTO /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_mkdir (const vfs_path_t *vpath, mode_t mode)
{
    (void) mode;                /* FIXME: should be used */

    return ftpfs_send_command (vpath, "MKD /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_rmdir (const vfs_path_t *vpath)
{
    return ftpfs_send_command (vpath, "RMD /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static vfs_file_handler_t *
ftpfs_fh_new (struct vfs_s_inode *ino, gboolean changed)
{
    ftp_file_handler_t *fh;

    fh = g_new0 (ftp_file_handler_t, 1);
    vfs_s_init_fh (VFS_FILE_HANDLER (fh), ino, changed);
    fh->sock = -1;

    return VFS_FILE_HANDLER (fh);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_fh_open (struct vfs_class *me, vfs_file_handler_t *fh, int flags, mode_t mode)
{
    ftp_file_handler_t *ftp = FTP_FILE_HANDLER (fh);

    (void) mode;

    /* File will be written only, so no need to retrieve it from ftp server */
    if (((flags & O_WRONLY) == O_WRONLY) && ((flags & (O_RDONLY | O_RDWR)) == 0))
    {
#ifdef HAVE_STRUCT_LINGER_L_LINGER
        struct linger li;
#else
        int li = 1;
#endif
        char *name;

        /* ftpfs_linear_start() called, so data will be written
         * to local temporary file and stored to ftp server
         * by vfs_s_close later
         */
        if (FTP_SUPER (VFS_FILE_HANDLER_SUPER (fh))->ctl_connection_busy)
        {
            if (fh->ino->localname == NULL)
            {
                vfs_path_t *vpath;
                int handle;

                handle = vfs_mkstemps (&vpath, me->name, fh->ino->ent->name);
                if (handle == -1)
                    return (-1);

                close (handle);
                fh->ino->localname = vfs_path_free (vpath, FALSE);
                ftp->append = (flags & O_APPEND) != 0;
            }
            return 0;
        }
        name = vfs_s_fullpath (me, fh->ino);
        if (name == NULL)
            return (-1);

        fh->handle =
            ftpfs_open_data_connection (me, VFS_FILE_HANDLER_SUPER (fh),
                                        (flags & O_APPEND) != 0 ? "APPE" : "STOR", name,
                                        TYPE_BINARY, 0);
        g_free (name);

        if (fh->handle < 0)
            return (-1);

#ifdef HAVE_STRUCT_LINGER_L_LINGER
        li.l_onoff = 1;
        li.l_linger = 120;
#endif
        setsockopt (fh->handle, SOL_SOCKET, SO_LINGER, &li, sizeof (li));

        if (fh->ino->localname != NULL)
        {
            unlink (fh->ino->localname);
            MC_PTR_FREE (fh->ino->localname);
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

static int
ftpfs_fh_close (struct vfs_class *me, vfs_file_handler_t *fh)
{
    if (fh->handle != -1 && fh->ino->localname == NULL)
    {
        ftp_super_t *ftp = FTP_SUPER (VFS_FILE_HANDLER_SUPER (fh));

        close (fh->handle);
        fh->handle = -1;
        ftp->ctl_connection_busy = FALSE;
        /* File is stored to destination already, so
         * we prevent VFS_SUBCLASS (me)->ftpfs_file_store() call from vfs_s_close ()
         */
        fh->changed = FALSE;
        if (ftpfs_get_reply (me, ftp->sock, NULL, 0) != COMPLETE)
            ERRNOR (EIO, -1);
        vfs_s_invalidate (me, VFS_FILE_HANDLER_SUPER (fh));
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_done (struct vfs_class *me)
{
    (void) me;

    g_slist_free_full (no_proxy, g_free);

    g_free (ftpfs_anonymous_passwd);
    g_free (ftpfs_proxy_host);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    for (iter = VFS_SUBCLASS (me)->supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;
        GString *name;

        name = vfs_path_element_build_pretty_path_str (super->path_element);

        func (name->str);
        g_string_free (name, TRUE);
    }
}

/* --------------------------------------------------------------------------------------------- */

static keyword_t
ftpfs_netrc_next (void)
{
    char *p;
    keyword_t i;
    static const char *const keywords[] = { "default", "machine",
        "login", "password", "passwd", "account", "macdef", NULL
    };

    while (TRUE)
    {
        netrcp = skip_separators (netrcp);
        if (*netrcp != '\n')
            break;
        netrcp++;
    }
    if (*netrcp == '\0')
        return NETRC_NONE;

    p = buffer;
    if (*netrcp == '"')
    {
        for (netrcp++; *netrcp != '"' && *netrcp != '\0'; netrcp++)
        {
            if (*netrcp == '\\')
                netrcp++;
            *p++ = *netrcp;
        }
    }
    else
    {
        for (; *netrcp != '\0' && !whiteness (*netrcp) && *netrcp != ','; netrcp++)
        {
            if (*netrcp == '\\')
                netrcp++;
            *p++ = *netrcp;
        }
    }

    *p = '\0';
    if (*buffer == '\0')
        return NETRC_NONE;

    for (i = NETRC_DEFAULT; keywords[i - 1] != NULL; i++)
        if (strcmp (keywords[i - 1], buffer) == 0)
            return i;

    return NETRC_UNKNOWN;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftpfs_netrc_bad_mode (const char *netrcname)
{
    struct stat mystat;

    if (stat (netrcname, &mystat) >= 0 && (mystat.st_mode & 077) != 0)
    {
        static gboolean be_angry = TRUE;

        if (be_angry)
        {
            message (D_ERROR, MSG_ERROR,
                     _("~/.netrc file has incorrect mode\nRemove password or correct mode"));
            be_angry = FALSE;
        }
        return TRUE;
    }

    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/* Scan .netrc until we find matching "machine" or "default"
 * domain is used for additional matching
 * No search is done after "default" in compliance with "man netrc"
 * Return TRUE if found, FALSE otherwise */

static gboolean
ftpfs_find_machine (const char *host, const char *domain)
{
    keyword_t keyword;

    if (host == NULL)
        host = "";
    if (domain == NULL)
        domain = "";

    while ((keyword = ftpfs_netrc_next ()) != NETRC_NONE)
    {
        if (keyword == NETRC_DEFAULT)
            return TRUE;

        if (keyword == NETRC_MACDEF)
        {
            /* Scan for an empty line, which concludes "macdef" */
            do
            {
                while (*netrcp != '\0' && *netrcp != '\n')
                    netrcp++;
                if (*netrcp != '\n')
                    break;
                netrcp++;
            }
            while (*netrcp != '\0' && *netrcp != '\n');

            continue;
        }

        if (keyword != NETRC_MACHINE)
            continue;

        /* Take machine name */
        if (ftpfs_netrc_next () == NETRC_NONE)
            break;

        if (g_ascii_strcasecmp (host, buffer) != 0)
        {
            const char *host_domain;

            /* Try adding our domain to short names in .netrc */
            host_domain = strchr (host, '.');
            if (host_domain == NULL)
                continue;

            /* Compare domain part */
            if (g_ascii_strcasecmp (host_domain, domain) != 0)
                continue;

            /* Compare local part */
            if (g_ascii_strncasecmp (host, buffer, host_domain - host) != 0)
                continue;
        }

        return TRUE;
    }

    /* end of .netrc */
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/* Extract login and password from .netrc for the host.
 * pass may be NULL.
 * Returns TRUE for success, FALSE for error */

static gboolean
ftpfs_netrc_lookup (const char *host, char **login, char **pass)
{
    char *netrcname;
    char *tmp_pass = NULL;
    char hostname[MAXHOSTNAMELEN];
    const char *domain;
    static struct rupcache
    {
        struct rupcache *next;
        char *host;
        char *login;
        char *pass;
    } *rup_cache = NULL, *rupp;

    /* Initialize *login and *pass */
    MC_PTR_FREE (*login);
    MC_PTR_FREE (*pass);

    /* Look up in the cache first */
    for (rupp = rup_cache; rupp != NULL; rupp = rupp->next)
        if (strcmp (host, rupp->host) == 0)
        {
            *login = g_strdup (rupp->login);
            *pass = g_strdup (rupp->pass);
            return TRUE;
        }

    /* Load current .netrc */
    netrcname = g_build_filename (mc_config_get_home_dir (), ".netrc", (char *) NULL);
    if (!g_file_get_contents (netrcname, &netrc, NULL, NULL))
    {
        g_free (netrcname);
        return TRUE;
    }

    netrcp = netrc;

    /* Find our own domain name */
    if (gethostname (hostname, sizeof (hostname)) < 0)
        *hostname = '\0';

    domain = strchr (hostname, '.');
    if (domain == NULL)
        domain = "";

    /* Scan for "default" and matching "machine" keywords */
    ftpfs_find_machine (host, domain);

    /* Scan for keywords following "default" and "machine" */
    while (TRUE)
    {
        keyword_t keyword;

        gboolean need_break = FALSE;
        keyword = ftpfs_netrc_next ();

        switch (keyword)
        {
        case NETRC_LOGIN:
            if (ftpfs_netrc_next () == NETRC_NONE)
            {
                need_break = TRUE;
                break;
            }

            /* We have another name already - should not happen */
            if (*login != NULL)
            {
                need_break = TRUE;
                break;
            }

            /* We have login name now */
            *login = g_strdup (buffer);
            break;

        case NETRC_PASSWORD:
        case NETRC_PASSWD:
            if (ftpfs_netrc_next () == NETRC_NONE)
            {
                need_break = TRUE;
                break;
            }

            /* Ignore unsafe passwords */
            if (*login != NULL &&
                strcmp (*login, "anonymous") != 0 && strcmp (*login, "ftp") != 0
                && ftpfs_netrc_bad_mode (netrcname))
            {
                need_break = TRUE;
                break;
            }

            /* Remember password.  pass may be NULL, so use tmp_pass */
            if (tmp_pass == NULL)
                tmp_pass = g_strdup (buffer);
            break;

        case NETRC_ACCOUNT:
            /* "account" is followed by a token which we ignore */
            if (ftpfs_netrc_next () == NETRC_NONE)
            {
                need_break = TRUE;
                break;
            }

            /* Ignore account, but warn user anyways */
            ftpfs_netrc_bad_mode (netrcname);
            break;

        default:
            /* Unexpected keyword or end of file */
            need_break = TRUE;
            break;
        }

        if (need_break)
            break;
    }

    MC_PTR_FREE (netrc);
    g_free (netrcname);

    rupp = g_new (struct rupcache, 1);
    rupp->host = g_strdup (host);
    rupp->login = g_strdup (*login);
    rupp->pass = g_strdup (tmp_pass);

    rupp->next = rup_cache;
    rup_cache = rupp;

    *pass = tmp_pass;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** This routine is called as the last step in load_setup */
void
ftpfs_init_passwd (void)
{
    ftpfs_anonymous_passwd = load_anon_passwd ();

    if (ftpfs_anonymous_passwd == NULL)
    {
        /* If there is no anonymous ftp password specified
         * then we'll just use anonymous@
         * We don't send any other thing because:
         * - We want to remain anonymous
         * - We want to stop SPAM
         * - We don't want to let ftp sites to discriminate by the user,
         *   host or country.
         */
        ftpfs_anonymous_passwd = g_strdup ("anonymous@");
    }
}

/* --------------------------------------------------------------------------------------------- */

void
vfs_init_ftpfs (void)
{
    tcp_init ();

    vfs_init_subclass (&ftpfs_subclass, "ftpfs", VFSF_NOLINKS | VFSF_REMOTE | VFSF_USETMP, "ftp");
    vfs_ftpfs_ops->done = ftpfs_done;
    vfs_ftpfs_ops->fill_names = ftpfs_fill_names;
    vfs_ftpfs_ops->stat = ftpfs_stat;
    vfs_ftpfs_ops->lstat = ftpfs_lstat;
    vfs_ftpfs_ops->fstat = ftpfs_fstat;
    vfs_ftpfs_ops->chmod = ftpfs_chmod;
    vfs_ftpfs_ops->chown = ftpfs_chown;
    vfs_ftpfs_ops->unlink = ftpfs_unlink;
    vfs_ftpfs_ops->rename = ftpfs_rename;
    vfs_ftpfs_ops->mkdir = ftpfs_mkdir;
    vfs_ftpfs_ops->rmdir = ftpfs_rmdir;
    vfs_ftpfs_ops->ctl = ftpfs_ctl;
    ftpfs_subclass.archive_same = ftpfs_archive_same;
    ftpfs_subclass.new_archive = ftpfs_new_archive;
    ftpfs_subclass.open_archive = ftpfs_open_archive;
    ftpfs_subclass.free_archive = ftpfs_free_archive;
    ftpfs_subclass.fh_new = ftpfs_fh_new;
    ftpfs_subclass.fh_open = ftpfs_fh_open;
    ftpfs_subclass.fh_close = ftpfs_fh_close;
    ftpfs_subclass.dir_load = ftpfs_dir_load;
    ftpfs_subclass.file_store = ftpfs_file_store;
    ftpfs_subclass.linear_start = ftpfs_linear_start;
    ftpfs_subclass.linear_read = ftpfs_linear_read;
    ftpfs_subclass.linear_close = ftpfs_linear_close;
    vfs_register_class (vfs_ftpfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
