/*
   Virtual File System: FTP file system.

   Copyright (C) 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2008, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Written by:
   Ching Hui, 1995
   Jakub Jelinek, 1995
   Miguel de Icaza, 1995, 1996, 1997
   Norbert Warmuth, 1997
   Pavel Machek, 1998
   Yury V. Zaytsev, 2010
   Slava Zanko <slavazanko@gmail.com>, 2010
   Andrew Borodin <aborodin@vmail.ru>, 2010

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
            s = mc_build_filename ( qhome (*bucket), remote_path +3-f, NULL );
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
#include <sys/time.h>           /* gettimeofday() */
#include <inttypes.h>           /* uintmax_t */

#include "lib/global.h"
#include "lib/util.h"
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
int ftpfs_use_passive_connections = 1;
int ftpfs_use_passive_connections_over_proxy = 0;

/* Method used to get directory listings:
 * 1: try 'LIST -la <path>', if it fails
 *    fall back to CWD <path>; LIST
 * 0: always use CWD <path>; LIST
 */
int ftpfs_use_unix_list_options = 1;

/* First "CWD <path>", then "LIST -la ." */
int ftpfs_first_cd_then_ls = 1;

/* Use the ~/.netrc */
int ftpfs_use_netrc = 1;

/* Anonymous setup */
char *ftpfs_anonymous_passwd = NULL;
int ftpfs_directory_timeout = 900;

/* Proxy host */
char *ftpfs_proxy_host = NULL;

/* wether we have to use proxy by default? */
int ftpfs_always_use_proxy = 0;

int ftpfs_ignore_chattr_errors = 1;

/*** file scope macro definitions ****************************************************************/

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define UPLOAD_ZERO_LENGTH_FILE
#define SUP ((ftp_super_data_t *) super->data)
#define FH_SOCK ((ftp_fh_data_t *) fh->data)->sock

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

#define ABORT_TIMEOUT 5
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
    int sock;

    char *proxy;                /* proxy server, NULL if no proxy */
    int failed_on_login;        /* used to pass the failure reason to upper levels */
    int use_passive_connection;
    int remote_is_amiga;        /* No leading slash allowed for AmiTCP (Amiga) */
    int isbinary;
    int cwd_deferred;           /* current_directory was changed but CWD command hasn't
                                   been sent yet */
    int strict;                 /* ftp server doesn't understand
                                 * "LIST -la <path>"; use "CWD <path>"/
                                 * "LIST" instead
                                 */
    int ctl_connection_busy;
    char *current_dir;
} ftp_super_data_t;

typedef struct
{
    int sock;
    int append;
} ftp_fh_data_t;

/*** file scope variables ************************************************************************/

static int ftpfs_errno;
static int code;

#ifdef FIXME_LATER_ALIGATOR
static struct linklist *connections_list;
#endif

static char reply_str[80];

static struct vfs_class vfs_ftpfs_ops;

static GSList *no_proxy;

static char buffer[BUF_MEDIUM];
static char *netrc;
static const char *netrcp;

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* char *ftpfs_translate_path (struct ftpfs_connection *bucket, char *remote_path)
   Translate a Unix path, i.e. MC's internal path representation (e.g.
   /somedir/somefile) to a path valid for the remote server. Every path
   transfered to the remote server has to be mangled by this function
   right prior to sending it.
   Currently only Amiga ftp servers are handled in a special manner.

   When the remote server is an amiga:
   a) strip leading slash if necesarry
   b) replace first occurance of ":/" with ":"
   c) strip trailing "/."
 */

static char *ftpfs_get_current_directory (struct vfs_class *me, struct vfs_s_super *super);
static int ftpfs_chdir_internal (struct vfs_class *me, struct vfs_s_super *super,
                                 const char *remote_path);
static int ftpfs_command (struct vfs_class *me, struct vfs_s_super *super, int wait_reply,
                          const char *fmt, ...) __attribute__ ((format (__printf__, 4, 5)));
static int ftpfs_open_socket (struct vfs_class *me, struct vfs_s_super *super);
static int ftpfs_login_server (struct vfs_class *me, struct vfs_s_super *super,
                               const char *netrcpass);
static int ftpfs_netrc_lookup (const char *host, char **login, char **pass);

/* --------------------------------------------------------------------------------------------- */

static char *
ftpfs_translate_path (struct vfs_class *me, struct vfs_s_super *super, const char *remote_path)
{
    if (!SUP->remote_is_amiga)
        return g_strdup (remote_path);
    else
    {
        char *ret, *p;

        if (MEDATA->logfile)
        {
            fprintf (MEDATA->logfile, "MC -- ftpfs_translate_path: %s\n", remote_path);
            fflush (MEDATA->logfile);
        }

        /* strip leading slash(es) */
        while (*remote_path == '/')
            remote_path++;

        /*
         * Don't change "/" into "", e.g. "CWD " would be
         * invalid.
         */
        if (*remote_path == '\0')
            return g_strdup (".");

        ret = g_strdup (remote_path);

        /* replace first occurance of ":/" with ":" */
        p = strchr (ret, ':');
        if ((p != NULL) && (*(p + 1) == '/'))
            memmove (p + 1, p + 2, strlen (p + 2) + 1);

        /* strip trailing "/." */
        p = strrchr (ret, '/');
        if ((p != NULL) && (*(p + 1) == '.') && (*(p + 2) == '\0'))
            *p = '\0';

        return ret;
    }
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
ftpfs_correct_url_parameters (const vfs_path_element_t * velement)
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
    if (ftpfs_use_netrc && path_element->user != NULL && path_element->password != NULL)
    {
        char *new_user = NULL;
        char *new_passwd = NULL;

        ftpfs_netrc_lookup (path_element->host, &new_user, &new_passwd);

        /* If user is different, remove password */
        if (new_user != NULL && strcmp (path_element->user, new_user) != 0)
        {
            g_free (path_element->password);
            path_element->password = NULL;
        }

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
    char answer[BUF_1K];
    int i;

    while (TRUE)
    {
        if (!vfs_s_get_line (me, sock, answer, sizeof (answer), '\n'))
        {
            if (string_buf != NULL)
                *string_buf = '\0';
            code = 421;
            return 4;
        }
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
                    if (!vfs_s_get_line (me, sock, answer, sizeof (answer), '\n'))
                    {
                        if (string_buf != NULL)
                            *string_buf = '\0';
                        code = 421;
                        return 4;
                    }
                    if ((sscanf (answer, "%d", &i) > 0) && (code == i) && (answer[3] == ' '))
                        break;
                }
            }
            if (string_buf != NULL)
                g_strlcpy (string_buf, answer, string_len);
            return code / 100;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_reconnect (struct vfs_class *me, struct vfs_s_super *super)
{
    int sock;

    sock = ftpfs_open_socket (me, super);
    if (sock != -1)
    {
        char *cwdir = SUP->current_dir;

        close (SUP->sock);
        SUP->sock = sock;
        SUP->current_dir = NULL;

        if (ftpfs_login_server (me, super, super->path_element->password) != 0)
        {
            if (cwdir == NULL)
                return 1;
            sock = ftpfs_chdir_internal (me, super, cwdir);
            g_free (cwdir);
            return sock == COMPLETE ? 1 : 0;
        }

        SUP->current_dir = cwdir;
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_command (struct vfs_class *me, struct vfs_s_super *super, int wait_reply, const char *fmt,
               ...)
{
    va_list ap;
    char *cmdstr;
    int status, cmdlen;
    static int retry = 0;
    static int level = 0;       /* ftpfs_login_server() use ftpfs_command() */

    va_start (ap, fmt);
    cmdstr = g_strdup_vprintf (fmt, ap);
    va_end (ap);

    cmdlen = strlen (cmdstr);
    cmdstr = g_realloc (cmdstr, cmdlen + 3);
    strcpy (cmdstr + cmdlen, "\r\n");
    cmdlen += 2;

    if (MEDATA->logfile)
    {
        if (strncmp (cmdstr, "PASS ", 5) == 0)
        {
            fputs ("PASS <Password not logged>\r\n", MEDATA->logfile);
        }
        else
        {
            size_t ret;
            ret = fwrite (cmdstr, cmdlen, 1, MEDATA->logfile);
            (void) ret;
        }

        fflush (MEDATA->logfile);
    }

    got_sigpipe = 0;
    tty_enable_interrupt_key ();
    status = write (SUP->sock, cmdstr, cmdlen);

    if (status < 0)
    {
        code = 421;

        if (errno == EPIPE)
        {                       /* Remote server has closed connection */
            if (level == 0)
            {
                level = 1;
                status = ftpfs_reconnect (me, super);
                level = 0;
                if (status && (write (SUP->sock, cmdstr, cmdlen) > 0))
                {
                    goto ok;
                }

            }
            got_sigpipe = 1;
        }
        g_free (cmdstr);
        tty_disable_interrupt_key ();
        return TRANSIENT;
    }
    retry = 0;
  ok:
    tty_disable_interrupt_key ();

    if (wait_reply)
    {
        status = ftpfs_get_reply (me, SUP->sock,
                                  (wait_reply & WANT_STRING) ? reply_str : NULL,
                                  sizeof (reply_str) - 1);
        if ((wait_reply & WANT_STRING) && !retry && !level && code == 421)
        {
            retry = 1;
            level = 1;
            status = ftpfs_reconnect (me, super);
            level = 0;
            if (status && (write (SUP->sock, cmdstr, cmdlen) > 0))
            {
                goto ok;
            }
        }
        retry = 0;
        g_free (cmdstr);
        return status;
    }
    g_free (cmdstr);
    return COMPLETE;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_free_archive (struct vfs_class *me, struct vfs_s_super *super)
{
    if (SUP->sock != -1)
    {
        vfs_print_message (_("ftpfs: Disconnecting from %s"), super->path_element->host);
        ftpfs_command (me, super, NONE, "QUIT");
        close (SUP->sock);
    }
    g_free (SUP->current_dir);
    g_free (super->data);
    super->data = NULL;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_changetype (struct vfs_class *me, struct vfs_s_super *super, int binary)
{
    if (binary != SUP->isbinary)
    {
        if (ftpfs_command (me, super, WAIT_REPLY, "TYPE %c", binary ? 'I' : 'A') != COMPLETE)
            ERRNOR (EIO, -1);
        SUP->isbinary = binary;
    }
    return binary;
}

/* --------------------------------------------------------------------------------------------- */
/* This routine logs the user in */

static int
ftpfs_login_server (struct vfs_class *me, struct vfs_s_super *super, const char *netrcpass)
{
    char *pass;
    char *op;
    char *name;                 /* login user name */
    int anon = 0;
    char reply_string[BUF_MEDIUM];

    SUP->isbinary = TYPE_UNKNOWN;

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
        anon = 1;
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

    if (!anon || MEDATA->logfile)
        pass = op;
    else
    {
        pass = g_strconcat ("-", op, (char *) NULL);
        wipe_password (op);
    }

    /* Proxy server accepts: username@host-we-want-to-connect */
    if (SUP->proxy)
        name =
            g_strconcat (super->path_element->user, "@",
                         super->path_element->host[0] ==
                         '!' ? super->path_element->host + 1 : super->path_element->host,
                         (char *) NULL);
    else
        name = g_strdup (super->path_element->user);

    if (ftpfs_get_reply (me, SUP->sock, reply_string, sizeof (reply_string) - 1) == COMPLETE)
    {
        char *reply_up;

        reply_up = g_ascii_strup (reply_string, -1);
        SUP->remote_is_amiga = strstr (reply_up, "AMIGA") != 0;
        if (strstr (reply_up, " SPFTP/1.0.0000 SERVER "))       /* handles `LIST -la` in a weird way */
            SUP->strict = RFC_STRICT;
        g_free (reply_up);

        if (MEDATA->logfile)
        {
            fprintf (MEDATA->logfile, "MC -- remote_is_amiga =  %d\n", SUP->remote_is_amiga);
            fflush (MEDATA->logfile);
        }

        vfs_print_message (_("ftpfs: sending login name"));

        switch (ftpfs_command (me, super, WAIT_REPLY, "USER %s", name))
        {
        case CONTINUE:
            vfs_print_message (_("ftpfs: sending user password"));
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
                vfs_print_message (_("ftpfs: sending user account"));
                code = ftpfs_command (me, super, WAIT_REPLY, "ACCT %s", op);
                g_free (op);
            }
            if (code != COMPLETE)
                break;
            /* fall through */

        case COMPLETE:
            vfs_print_message (_("ftpfs: logged in"));
            wipe_password (pass);
            g_free (name);
            return 1;

        default:
            SUP->failed_on_login = 1;
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
    ERRNOR (EPERM, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_load_no_proxy_list (void)
{
    /* FixMe: shouldn't be hardcoded!!! */
    char s[BUF_LARGE];          /* provide for BUF_LARGE characters */
    FILE *npf;
    int c;
    char *p;
    static char *mc_file = NULL;

    mc_file = g_build_filename (mc_global.sysconfig_dir, "mc.no_proxy", (char *) NULL);
    if (exist_file (mc_file))
    {
        npf = fopen (mc_file, "r");
        if (npf != NULL)
        {
            while (fgets (s, sizeof (s), npf) != NULL)
            {
                p = strchr (s, '\n');
                if (p == NULL)  /* skip bogus entries */
                {
                    while ((c = fgetc (npf)) != EOF && c != '\n')
                        ;
                    continue;
                }

                if (p != s)
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
/* Return 1 if FTP proxy should be used for this host, 0 otherwise */

static int
ftpfs_check_proxy (const char *host)
{
    GSList *npe;

    if (ftpfs_proxy_host == NULL || *ftpfs_proxy_host == '\0' || host == NULL || *host == '\0')
        return 0;               /* sanity check */

    if (*host == '!')
        return 1;

    if (!ftpfs_always_use_proxy)
        return 0;

    if (strchr (host, '.') == NULL)
        return 0;

    ftpfs_load_no_proxy_list ();
    for (npe = no_proxy; npe != NULL; npe = g_slist_next (npe))
    {
        const char *domain = (const char *) npe->data;

        if (domain[0] == '.')
        {
            size_t ld = strlen (domain);
            size_t lh = strlen (host);

            while (ld != 0 && lh != 0 && host[lh - 1] == domain[ld - 1])
            {
                ld--;
                lh--;
            }

            if (ld == 0)
                return 0;
        }
        else if (g_ascii_strcasecmp (host, domain) == 0)
            return 0;
    }

    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_get_proxy_host_and_port (const char *proxy, char **host, int *port)
{
    vfs_path_element_t *path_element;

    path_element = vfs_url_split (proxy, FTP_COMMAND_PORT, URL_USE_ANONYMOUS);
    *host = g_strdup (path_element->host);
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
    int tmp_port;
    int e;

    (void) me;

    /* Use a proxy host? */
    host = g_strdup (super->path_element->host);

    if (host == NULL || *host == '\0')
    {
        vfs_print_message (_("ftpfs: Invalid host name."));
        ftpfs_errno = EINVAL;
        g_free (host);
        return -1;
    }

    /* Hosts to connect to that start with a ! should use proxy */
    tmp_port = super->path_element->port;

    if (SUP->proxy != NULL)
        ftpfs_get_proxy_host_and_port (ftpfs_proxy_host, &host, &tmp_port);

    g_snprintf (port, sizeof (port), "%hu", (unsigned short) tmp_port);
    if (port[0] == '\0')
    {
        g_free (host);
        ftpfs_errno = errno;
        return -1;
    }

    tty_enable_interrupt_key ();        /* clear the interrupt flag */

    memset (&hints, 0, sizeof (struct addrinfo));
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
        ftpfs_errno = EINVAL;
        return -1;
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
            ftpfs_errno = errno;
            return -1;
        }

        vfs_print_message (_("ftpfs: making connection to %s"), host);
        g_free (host);
        host = NULL;

        if (connect (my_socket, curr_res->ai_addr, curr_res->ai_addrlen) >= 0)
            break;

        ftpfs_errno = errno;
        close (my_socket);

        if (errno == EINTR && tty_got_interrupt ())
            vfs_print_message (_("ftpfs: connection interrupted by user"));
        else if (res->ai_next == NULL)
            vfs_print_message (_("ftpfs: connection to server failed: %s"),
                               unix_error_string (errno));
        else
            continue;

        freeaddrinfo (res);
        tty_disable_interrupt_key ();
        return -1;
    }

    freeaddrinfo (res);
    tty_disable_interrupt_key ();
    return my_socket;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_archive_int (struct vfs_class *me, struct vfs_s_super *super)
{
    int retry_seconds = 0;
    int count_down;

    /* We do not want to use the passive if we are using proxies */
    if (SUP->proxy)
        SUP->use_passive_connection = ftpfs_use_passive_connections_over_proxy;

    do
    {
        SUP->failed_on_login = 0;

        SUP->sock = ftpfs_open_socket (me, super);
        if (SUP->sock == -1)
            return -1;

        if (ftpfs_login_server (me, super, NULL) != 0)
        {
            /* Logged in, no need to retry the connection */
            break;
        }
        else
        {
            if (!SUP->failed_on_login)
                return -1;

            /* Close only the socket descriptor */
            close (SUP->sock);

            if (ftpfs_retry_seconds != 0)
            {
                retry_seconds = ftpfs_retry_seconds;
                tty_enable_interrupt_key ();
                for (count_down = retry_seconds; count_down; count_down--)
                {
                    vfs_print_message (_("Waiting to retry... %d (Control-G to cancel)"),
                                       count_down);
                    sleep (1);
                    if (tty_got_interrupt ())
                    {
                        /* ftpfs_errno = E; */
                        tty_disable_interrupt_key ();
                        return 0;
                    }
                }
                tty_disable_interrupt_key ();
            }
        }
    }
    while (retry_seconds != 0);

    SUP->current_dir = ftpfs_get_current_directory (me, super);
    if (SUP->current_dir == NULL)
        SUP->current_dir = g_strdup (PATH_SEP_STR);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_archive (struct vfs_s_super *super,
                    const vfs_path_t * vpath, const vfs_path_element_t * vpath_element)
{
    (void) vpath;

    super->data = g_new0 (ftp_super_data_t, 1);

    super->path_element = ftpfs_correct_url_parameters (vpath_element);
    SUP->proxy = NULL;
    if (ftpfs_check_proxy (super->path_element->host))
        SUP->proxy = ftpfs_proxy_host;
    SUP->use_passive_connection = ftpfs_use_passive_connections;
    SUP->strict = ftpfs_use_unix_list_options ? RFC_AUTODETECT : RFC_STRICT;
    SUP->isbinary = TYPE_UNKNOWN;
    SUP->remote_is_amiga = 0;
    super->name = g_strdup ("/");
    super->root =
        vfs_s_new_inode (vpath_element->class, super,
                         vfs_s_default_stat (vpath_element->class, S_IFDIR | 0755));

    return ftpfs_open_archive_int (vpath_element->class, super);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_archive_same (const vfs_path_element_t * vpath_element, struct vfs_s_super *super,
                    const vfs_path_t * vpath, void *cookie)
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

    if (ftpfs_command (me, super, NONE, "PWD") == COMPLETE &&
        ftpfs_get_reply (me, SUP->sock, buf, sizeof (buf)) == COMPLETE)
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
                        if (*(bufq - 1) != '/')
                        {
                            *bufq++ = '/';
                            *bufq = '\0';
                        }

                        if (*bufp == '/')
                            return g_strdup (bufp);

                        /* If the remote server is an Amiga a leading slash
                           might be missing. MC needs it because it is used
                           as separator between hostname and path internally. */
                        return g_strconcat ("/", bufp, (char *) NULL);
                    }

                    break;
                }
            }
    }

    ftpfs_errno = EIO;
    return NULL;
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive PASV FTP connection */

static int
ftpfs_setup_passive_pasv (struct vfs_class *me, struct vfs_s_super *super,
                          int my_socket, struct sockaddr_storage *sa, socklen_t * salen)
{
    char *c;
    char n[6];
    int xa, xb, xc, xd, xe, xf;

    if (ftpfs_command (me, super, WAIT_REPLY | WANT_STRING, "PASV") != COMPLETE)
        return 0;

    /* Parse remote parameters */
    for (c = reply_str + 4; (*c) && (!isdigit ((unsigned char) *c)); c++);

    if (!*c)
        return 0;
    if (!isdigit ((unsigned char) *c))
        return 0;
    if (sscanf (c, "%d,%d,%d,%d,%d,%d", &xa, &xb, &xc, &xd, &xe, &xf) != 6)
        return 0;

    n[0] = (unsigned char) xa;
    n[1] = (unsigned char) xb;
    n[2] = (unsigned char) xc;
    n[3] = (unsigned char) xd;
    n[4] = (unsigned char) xe;
    n[5] = (unsigned char) xf;

    memcpy (&(((struct sockaddr_in *) sa)->sin_addr.s_addr), (void *) n, 4);
    memcpy (&(((struct sockaddr_in *) sa)->sin_port), (void *) &n[4], 2);

    if (connect (my_socket, (struct sockaddr *) sa, *salen) < 0)
        return 0;

    return 1;
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive EPSV FTP connection */

static int
ftpfs_setup_passive_epsv (struct vfs_class *me, struct vfs_s_super *super,
                          int my_socket, struct sockaddr_storage *sa, socklen_t * salen)
{
    char *c;
    int port;

    if (ftpfs_command (me, super, WAIT_REPLY | WANT_STRING, "EPSV") != COMPLETE)
        return 0;

    /* (|||<port>|) */
    c = strchr (reply_str, '|');
    if (c == NULL)
        return 0;
    if (strlen (c) > 3)
        c += 3;
    else
        return 0;

    port = atoi (c);
    if (port < 0 || port > 65535)
        return 0;
    port = htons (port);

    switch (sa->ss_family)
    {
    case AF_INET:
        ((struct sockaddr_in *) sa)->sin_port = port;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *) sa)->sin6_port = port;
        break;
    }

    return (connect (my_socket, (struct sockaddr *) sa, *salen) < 0) ? 0 : 1;
}

/* --------------------------------------------------------------------------------------------- */
/* Setup Passive ftp connection, we use it for source routed connections */

static int
ftpfs_setup_passive (struct vfs_class *me, struct vfs_s_super *super,
                     int my_socket, struct sockaddr_storage *sa, socklen_t * salen)
{
    /* It's IPV4, so try PASV first, some servers and ALGs get confused by EPSV */
    if (sa->ss_family == AF_INET)
    {
        if (!ftpfs_setup_passive_pasv (me, super, my_socket, sa, salen))
            /* An IPV4 FTP server might support EPSV, so if PASV fails we can try EPSV anyway */
            if (!ftpfs_setup_passive_epsv (me, super, my_socket, sa, salen))
                return 0;
    }
    /* It's IPV6, so EPSV is our only hope */
    else
    {
        if (!ftpfs_setup_passive_epsv (me, super, my_socket, sa, salen))
            return 0;
    }

    return 1;
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
        /* Not implemented */
    default:
        return 0;
    }

    addr = g_try_malloc (NI_MAXHOST);
    if (addr == NULL)
        ERRNOR (ENOMEM, -1);

    if (getnameinfo
        ((struct sockaddr *) &data_addr, data_addrlen, addr, NI_MAXHOST, NULL, 0,
         NI_NUMERICHOST) != 0)
    {
        g_free (addr);
        ERRNOR (EIO, -1);
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
    if (ftpfs_command (me, super, WAIT_REPLY, "EPRT |%u|%s|%hu|", af, addr, port) == COMPLETE)
    {
        g_free (addr);
        return 1;
    }

    g_free (addr);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/* Initialize a socket for FTP DATA connection */

static int
ftpfs_init_data_socket (struct vfs_class *me, struct vfs_s_super *super,
                        struct sockaddr_storage *data_addr, socklen_t * data_addrlen)
{
    int result;

    memset (data_addr, 0, sizeof (struct sockaddr_storage));
    *data_addrlen = sizeof (struct sockaddr_storage);

    if (SUP->use_passive_connection)
        result = getpeername (SUP->sock, (struct sockaddr *) data_addr, data_addrlen);
    else
        result = getsockname (SUP->sock, (struct sockaddr *) data_addr, data_addrlen);

    if (result == -1)
        return -1;

    switch (data_addr->ss_family)
    {
    case AF_INET:
        ((struct sockaddr_in *) data_addr)->sin_port = 0;
        break;
    case AF_INET6:
        ((struct sockaddr_in6 *) data_addr)->sin6_port = 0;
        break;
    default:
        vfs_print_message (_("ftpfs: invalid address family"));
        ERRNOR (EINVAL, -1);
    }

    result = socket (data_addr->ss_family, SOCK_STREAM, IPPROTO_TCP);

    if (result < 0)
    {
        vfs_print_message (_("ftpfs: could not create socket: %s"), unix_error_string (errno));
        return -1;
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */
/* Initialize FTP DATA connection */

static int
ftpfs_initconn (struct vfs_class *me, struct vfs_s_super *super)
{
    struct sockaddr_storage data_addr;
    socklen_t data_addrlen;

    /*
     * Don't factor socket initialization out of these conditionals,
     * because ftpfs_init_data_socket initializes it in different way
     * depending on use_passive_connection flag.
     */

    /* Try to establish a passive connection first (if requested) */
    if (SUP->use_passive_connection)
    {
        int data_sock;

        data_sock = ftpfs_init_data_socket (me, super, &data_addr, &data_addrlen);
        if (data_sock < 0)
            return -1;

        if (ftpfs_setup_passive (me, super, data_sock, &data_addr, &data_addrlen))
            return data_sock;

        vfs_print_message (_("ftpfs: could not setup passive mode"));
        SUP->use_passive_connection = 0;

        close (data_sock);
    }

    /* If passive setup is diabled or failed, fallback to active connections */
    if (!SUP->use_passive_connection)
    {
        int data_sock;

        data_sock = ftpfs_init_data_socket (me, super, &data_addr, &data_addrlen);
        if (data_sock < 0)
            return -1;

        if ((bind (data_sock, (struct sockaddr *) &data_addr, data_addrlen) == 0) &&
            (getsockname (data_sock, (struct sockaddr *) &data_addr, &data_addrlen) == 0) &&
            (listen (data_sock, 1) == 0) &&
            (ftpfs_setup_active (me, super, data_addr, data_addrlen) != 0))
            return data_sock;

        close (data_sock);
    }

    /* Restore the initial value of use_passive_connection (for subsequent retries) */
    SUP->use_passive_connection = SUP->proxy != NULL ? ftpfs_use_passive_connections_over_proxy :
        ftpfs_use_passive_connections;

    ftpfs_errno = EIO;
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_open_data_connection (struct vfs_class *me, struct vfs_s_super *super, const char *cmd,
                            const char *remote, int isbinary, int reget)
{
    struct sockaddr_storage from;
    int s, j, data;
    socklen_t fromlen = sizeof (from);

    s = ftpfs_initconn (me, super);
    if (s == -1)
        return -1;

    if (ftpfs_changetype (me, super, isbinary) == -1)
        return -1;
    if (reget > 0)
    {
        j = ftpfs_command (me, super, WAIT_REPLY, "REST %d", reget);
        if (j != CONTINUE)
            return -1;
    }
    if (remote)
    {
        char *remote_path = ftpfs_translate_path (me, super, remote);
        j = ftpfs_command (me, super, WAIT_REPLY, "%s /%s", cmd,
                           /* WarFtpD can't STORE //filename */
                           (*remote_path == '/') ? remote_path + 1 : remote_path);
        g_free (remote_path);
    }
    else
        j = ftpfs_command (me, super, WAIT_REPLY, "%s", cmd);

    if (j != PRELIM)
        ERRNOR (EPERM, -1);
    tty_enable_interrupt_key ();
    if (SUP->use_passive_connection)
        data = s;
    else
    {
        data = accept (s, (struct sockaddr *) &from, &fromlen);
        if (data < 0)
        {
            ftpfs_errno = errno;
            close (s);
            return -1;
        }
        close (s);
    }
    tty_disable_interrupt_key ();
    return data;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_linear_abort (struct vfs_class *me, vfs_file_handler_t * fh)
{
    struct vfs_s_super *super = FH_SUPER;
    static unsigned char const ipbuf[3] = { IAC, IP, IAC };
    fd_set mask;
    char buf[BUF_8K];
    int dsock = FH_SOCK;
    FH_SOCK = -1;
    SUP->ctl_connection_busy = 0;

    vfs_print_message (_("ftpfs: aborting transfer."));
    if (send (SUP->sock, ipbuf, sizeof (ipbuf), MSG_OOB) != sizeof (ipbuf))
    {
        vfs_print_message (_("ftpfs: abort error: %s"), unix_error_string (errno));
        if (dsock != -1)
            close (dsock);
        return;
    }

    if (ftpfs_command (me, super, NONE, "%cABOR", DM) != COMPLETE)
    {
        vfs_print_message (_("ftpfs: abort failed"));
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
            struct timeval start_tim, tim;
            gettimeofday (&start_tim, NULL);
            /* flush the remaining data */
            while (read (dsock, buf, sizeof (buf)) > 0)
            {
                gettimeofday (&tim, NULL);
                if (tim.tv_sec > start_tim.tv_sec + ABORT_TIMEOUT)
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
    if ((ftpfs_get_reply (me, SUP->sock, NULL, 0) == TRANSIENT) && (code == 426))
        ftpfs_get_reply (me, SUP->sock, NULL, 0);
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
    int depth;

    dir->symlink_status = FTPFS_RESOLVING_SYMLINKS;
    for (flist = dir->file_list->next; flist != dir->file_list; flist = flist->next)
    {
        /* flist->data->l_stat is alread initialized with 0 */
        fel = flist->data;
        if (S_ISLNK (fel->s.st_mode) && fel->linkname)
        {
            if (fel->linkname[0] == '/')
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
                    strcat (tmp, "/");
                strcat (tmp + 1, fel->linkname);
            }
            for (depth = 0; depth < 100; depth++)
            {                   /* depth protects against recursive symbolic links */
                canonicalize_pathname (tmp);
                fe = _get_file_entry (bucket, tmp, 0, 0);
                if (fe)
                {
                    if (S_ISLNK (fe->s.st_mode) && fe->l_stat == 0)
                    {
                        /* Symlink points to link which isn't resolved, yet. */
                        if (fe->linkname[0] == '/')
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
                            *(strrchr (tmp, '/') + 1) = '\0';   /* dirname */
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
    if (strchr (dir->remote_path, ' '))
    {
        if (ftpfs_chdir_internal (bucket, dir->remote_path) != COMPLETE)
        {
            vfs_print_message (_("ftpfs: CWD failed."));
            return;
        }
        sock = ftpfs_open_data_connection (bucket, "LIST -lLa", ".", TYPE_ASCII, 0);
    }
    else
        sock = ftpfs_open_data_connection (bucket, "LIST -lLa", dir->remote_path, TYPE_ASCII, 0);

    if (sock == -1)
    {
        vfs_print_message (_("ftpfs: couldn't resolve symlink"));
        return;
    }

    fp = fdopen (sock, "r");
    if (fp == NULL)
    {
        close (sock);
        vfs_print_message (_("ftpfs: couldn't resolve symlink"));
        return;
    }
    tty_enable_interrupt_key ();
    flist = dir->file_list->next;
    while (1)
    {
        do
        {
            if (flist == dir->file_list)
                goto done;
            fe = flist->data;
            flist = flist->next;
        }
        while (!S_ISLNK (fe->s.st_mode));
        while (1)
        {
            if (fgets (buffer, sizeof (buffer), fp) == NULL)
                goto done;
            if (MEDATA->logfile)
            {
                fputs (buffer, MEDATA->logfile);
                fflush (MEDATA->logfile);
            }
            vfs_die ("This code should be commented out\n");
            if (vfs_parse_ls_lga (buffer, &s, &filename, NULL))
            {
                int r = strcmp (fe->name, filename);
                g_free (filename);
                if (r == 0)
                {
                    if (S_ISLNK (s.st_mode))
                    {
                        /* This server doesn't understand LIST -lLa */
                        switch_method = 1;
                        goto done;
                    }
                    fe->l_stat = g_new (struct stat, 1);
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
    while (fgets (buffer, sizeof (buffer), fp) != NULL);
    tty_disable_interrupt_key ();
    fclose (fp);
    ftpfs_get_reply (me, SUP->sock, NULL, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
resolve_symlink (struct vfs_class *me, struct vfs_s_super *super, struct vfs_s_inode *dir)
{
    vfs_print_message (_("Resolving symlink..."));

    if (SUP->strict_rfc959_list_cmd)
        resolve_symlink_without_ls_options (me, super, dir);
    else
        resolve_symlink_with_ls_options (me, super, dir);
}
#endif

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_dir_load (struct vfs_class *me, struct vfs_s_inode *dir, char *remote_path)
{
    struct vfs_s_entry *ent;
    struct vfs_s_super *super = dir->super;
    int sock, num_entries = 0;
    char lc_buffer[BUF_8K];
    int cd_first;

    cd_first = ftpfs_first_cd_then_ls || (SUP->strict == RFC_STRICT)
        || (strchr (remote_path, ' ') != NULL);

  again:
    vfs_print_message (_("ftpfs: Reading FTP directory %s... %s%s"),
                       remote_path,
                       SUP->strict ==
                       RFC_STRICT ? _("(strict rfc959)") : "", cd_first ? _("(chdir first)") : "");

    if (cd_first)
    {
        if (ftpfs_chdir_internal (me, super, remote_path) != COMPLETE)
        {
            ftpfs_errno = ENOENT;
            vfs_print_message (_("ftpfs: CWD failed."));
            return -1;
        }
    }

    gettimeofday (&dir->timestamp, NULL);
    dir->timestamp.tv_sec += ftpfs_directory_timeout;

    if (SUP->strict == RFC_STRICT)
        sock = ftpfs_open_data_connection (me, super, "LIST", 0, TYPE_ASCII, 0);
    else if (cd_first)
        /* Dirty hack to avoid autoprepending / to . */
        /* Wu-ftpd produces strange output for '/' if 'LIST -la .' used */
        sock = ftpfs_open_data_connection (me, super, "LIST -la", 0, TYPE_ASCII, 0);
    else
    {
        char *path;

        /* Trailing "/." is necessary if remote_path is a symlink */
        path = mc_build_filename (remote_path, ".", NULL);
        sock = ftpfs_open_data_connection (me, super, "LIST -la", path, TYPE_ASCII, 0);
        g_free (path);
    }

    if (sock == -1)
        goto fallback;

    /* Clear the interrupt flag */
    tty_enable_interrupt_key ();

    vfs_parse_ls_lga_init ();
    while (1)
    {
        int i;
        size_t count_spaces = 0;
        int res = vfs_s_get_line_interruptible (me, lc_buffer, sizeof (lc_buffer),
                                                sock);
        if (!res)
            break;

        if (res == EINTR)
        {
            me->verrno = ECONNRESET;
            close (sock);
            tty_disable_interrupt_key ();
            ftpfs_get_reply (me, SUP->sock, NULL, 0);
            vfs_print_message (_("%s: failure"), me->name);
            return -1;
        }

        if (MEDATA->logfile)
        {
            fputs (lc_buffer, MEDATA->logfile);
            fputs ("\n", MEDATA->logfile);
            fflush (MEDATA->logfile);
        }

        ent = vfs_s_generate_entry (me, NULL, dir, 0);
        i = ent->ino->st.st_nlink;
        if (!vfs_parse_ls_lga
            (lc_buffer, &ent->ino->st, &ent->name, &ent->ino->linkname, &count_spaces))
        {
            vfs_s_free_entry (me, ent);
            continue;
        }
        ent->ino->st.st_nlink = i;      /* Ouch, we need to preserve our counts :-( */
        num_entries++;
        vfs_s_store_filename_leading_spaces (ent, count_spaces);
        vfs_s_insert_entry (me, dir, ent);
    }

    close (sock);
    me->verrno = E_REMOTE;
    if ((ftpfs_get_reply (me, SUP->sock, NULL, 0) != COMPLETE))
        goto fallback;

    if (num_entries == 0 && cd_first == 0)
    {
        /* The LIST command may produce an empty output. In such scenario
           it is not clear whether this is caused by  `remote_path' being
           a non-existent path or for some other reason (listing emtpy
           directory without the -a option, non-readable directory, etc.).

           Since `dir_load' is a crucial method, when it comes to determine
           whether a given path is a _directory_, the code must try its best
           to determine the type of `remote_path'. The only reliable way to
           achieve this is trough issuing a CWD command. */

        cd_first = 1;
        goto again;
    }

    vfs_s_normalize_filename_leading_spaces (dir, vfs_parse_ls_lga_get_final_spaces ());

    if (SUP->strict == RFC_AUTODETECT)
        SUP->strict = RFC_DARING;

    vfs_print_message (_("%s: done."), me->name);
    return 0;

  fallback:
    if (SUP->strict == RFC_AUTODETECT)
    {
        /* It's our first attempt to get a directory listing from this
           server (UNIX style LIST command) */
        SUP->strict = RFC_STRICT;
        /* I hate goto, but recursive call needs another 8K on stack */
        /* return ftpfs_dir_load (me, dir, remote_path); */
        cd_first = 1;
        goto again;
    }
    vfs_print_message (_("ftpfs: failed; nowhere to fallback to"));
    ERRNOR (EACCES, -1);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_file_store (struct vfs_class *me, vfs_file_handler_t * fh, char *name, char *localname)
{
    int h, sock;
    off_t n_stored;
#ifdef HAVE_STRUCT_LINGER_L_LINGER
    struct linger li;
#else
    int flag_one = 1;
#endif
    char lc_buffer[BUF_8K];
    struct stat s;
    char *w_buf;
    struct vfs_s_super *super = FH_SUPER;
    ftp_fh_data_t *ftp = (ftp_fh_data_t *) fh->data;

    h = open (localname, O_RDONLY);
    if (h == -1)
        ERRNOR (EIO, -1);

    sock =
        ftpfs_open_data_connection (me, super, ftp->append ? "APPE" : "STOR", name, TYPE_BINARY, 0);
    if (sock < 0 || fstat (h, &s) == -1)
    {
        close (h);
        return -1;
    }
#ifdef HAVE_STRUCT_LINGER_L_LINGER
    li.l_onoff = 1;
    li.l_linger = 120;
    setsockopt (sock, SOL_SOCKET, SO_LINGER, (char *) &li, sizeof (li));
#else
    setsockopt (sock, SOL_SOCKET, SO_LINGER, &flag_one, sizeof (flag_one));
#endif
    n_stored = 0;

    tty_enable_interrupt_key ();
    while (TRUE)
    {
        ssize_t n_read, n_written;

        while ((n_read = read (h, lc_buffer, sizeof (lc_buffer))) == -1)
        {
            if (errno != EINTR)
            {
                ftpfs_errno = errno;
                goto error_return;
            }
            if (tty_got_interrupt ())
            {
                ftpfs_errno = EINTR;
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

                ftpfs_errno = errno;
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
    close (h);
    if (ftpfs_get_reply (me, SUP->sock, NULL, 0) != COMPLETE)
        ERRNOR (EIO, -1);
    return 0;
  error_return:
    tty_disable_interrupt_key ();
    close (sock);
    close (h);
    ftpfs_get_reply (me, SUP->sock, NULL, 0);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_linear_start (struct vfs_class *me, vfs_file_handler_t * fh, off_t offset)
{
    char *name;

    if (fh->data == NULL)
        fh->data = g_new0 (ftp_fh_data_t, 1);

    name = vfs_s_fullpath (me, fh->ino);
    if (name == NULL)
        return 0;
    FH_SOCK = ftpfs_open_data_connection (me, FH_SUPER, "RETR", name, TYPE_BINARY, offset);
    g_free (name);
    if (FH_SOCK == -1)
        ERRNOR (EACCES, 0);
    fh->linear = LS_LINEAR_OPEN;
    ((ftp_super_data_t *) (FH_SUPER->data))->ctl_connection_busy = 1;
    ((ftp_fh_data_t *) fh->data)->append = 0;
    return 1;
}

/* --------------------------------------------------------------------------------------------- */

static ssize_t
ftpfs_linear_read (struct vfs_class *me, vfs_file_handler_t * fh, void *buf, size_t len)
{
    ssize_t n;
    struct vfs_s_super *super = FH_SUPER;

    while ((n = read (FH_SOCK, buf, len)) < 0)
    {
        if ((errno == EINTR) && !tty_got_interrupt ())
            continue;
        break;
    }

    if (n < 0)
        ftpfs_linear_abort (me, fh);

    if (n == 0)
    {
        SUP->ctl_connection_busy = 0;
        close (FH_SOCK);
        FH_SOCK = -1;
        if ((ftpfs_get_reply (me, SUP->sock, NULL, 0) != COMPLETE))
            ERRNOR (E_REMOTE, -1);
        return 0;
    }
    ERRNOR (errno, n);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_linear_close (struct vfs_class *me, vfs_file_handler_t * fh)
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
            int v;

            if (!FH->linear)
                vfs_die ("You may not do this");
            if (FH->linear == LS_LINEAR_CLOSED || FH->linear == LS_LINEAR_PREOPEN)
                return 0;

            v = vfs_s_select_on_two (((ftp_fh_data_t *) (FH->data))->sock, 0);
            return (((v < 0) && (errno == EINTR)) || v == 0) ? 1 : 0;
        }
    default:
        return 0;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_send_command (const vfs_path_t * vpath, const char *cmd, int flags)
{
    const char *rpath;
    char *p;
    struct vfs_s_super *super;
    int r;
    const vfs_path_element_t *path_element;
    int flush_directory_cache = (flags & OPT_FLUSH);

    path_element = vfs_path_get_by_index (vpath, -1);

    rpath = vfs_s_get_path (vpath, &super, 0);
    if (rpath == NULL)
        return -1;

    p = ftpfs_translate_path (path_element->class, super, rpath);
    r = ftpfs_command (path_element->class, super, WAIT_REPLY, cmd, p);
    g_free (p);
    vfs_stamp_create (&vfs_ftpfs_ops, super);
    if (flags & OPT_IGNORE_ERROR)
        r = COMPLETE;
    if (r != COMPLETE)
    {
        path_element->class->verrno = EPERM;
        return -1;
    }
    if (flush_directory_cache)
        vfs_s_invalidate (path_element->class, super);
    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chmod (const vfs_path_t * vpath, mode_t mode)
{
    char buf[BUF_SMALL];
    int ret;

    g_snprintf (buf, sizeof (buf), "SITE CHMOD %4.4o /%%s", (int) (mode & 07777));

    ret = ftpfs_send_command (vpath, buf, OPT_FLUSH);

    return ftpfs_ignore_chattr_errors ? 0 : ret;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chown (const vfs_path_t * vpath, uid_t owner, gid_t group)
{
#if 0
    (void) vpath;
    (void) owner;
    (void) group;

    ftpfs_errno = EPERM;
    return -1;
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
ftpfs_unlink (const vfs_path_t * vpath)
{
    return ftpfs_send_command (vpath, "DELE /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

/* Return 1 if path is the same directory as the one we are in now */
static int
ftpfs_is_same_dir (struct vfs_class *me, struct vfs_s_super *super, const char *path)
{
    (void) me;

    if (SUP->current_dir == NULL)
        return FALSE;
    return (strcmp (path, SUP->current_dir) == 0);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_chdir_internal (struct vfs_class *me, struct vfs_s_super *super, const char *remote_path)
{
    int r;
    char *p;

    if (!SUP->cwd_deferred && ftpfs_is_same_dir (me, super, remote_path))
        return COMPLETE;

    p = ftpfs_translate_path (me, super, remote_path);
    r = ftpfs_command (me, super, WAIT_REPLY, "CWD /%s", p);
    g_free (p);

    if (r != COMPLETE)
        ftpfs_errno = EIO;
    else
    {
        g_free (SUP->current_dir);
        SUP->current_dir = g_strdup (remote_path);
        SUP->cwd_deferred = 0;
    }
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_rename (const vfs_path_t * vpath1, const vfs_path_t * vpath2)
{
    ftpfs_send_command (vpath1, "RNFR /%s", OPT_FLUSH);
    return ftpfs_send_command (vpath2, "RNTO /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_mkdir (const vfs_path_t * vpath, mode_t mode)
{
    (void) mode;                /* FIXME: should be used */

    return ftpfs_send_command (vpath, "MKD /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_rmdir (const vfs_path_t * vpath)
{
    return ftpfs_send_command (vpath, "RMD /%s", OPT_FLUSH);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_fh_free_data (vfs_file_handler_t * fh)
{
    if (fh != NULL)
    {
        g_free (fh->data);
        fh->data = NULL;
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_fh_open (struct vfs_class *me, vfs_file_handler_t * fh, int flags, mode_t mode)
{
    ftp_fh_data_t *ftp;

    (void) mode;

    fh->data = g_new0 (ftp_fh_data_t, 1);
    ftp = (ftp_fh_data_t *) fh->data;
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
        if (((ftp_super_data_t *) (FH_SUPER->data))->ctl_connection_busy)
        {
            if (!fh->ino->localname)
            {
                vfs_path_t *vpath;
                int handle;

                handle = vfs_mkstemps (&vpath, me->name, fh->ino->ent->name);
                if (handle == -1)
                {
                    vfs_path_free (vpath);
                    goto fail;
                }
                close (handle);
                fh->ino->localname = vfs_path_to_str (vpath);
                vfs_path_free (vpath);
                ftp->append = flags & O_APPEND;
            }
            return 0;
        }
        name = vfs_s_fullpath (me, fh->ino);
        if (name == NULL)
            goto fail;
        fh->handle =
            ftpfs_open_data_connection (me, fh->ino->super,
                                        (flags & O_APPEND) ? "APPE" : "STOR", name, TYPE_BINARY, 0);
        g_free (name);

        if (fh->handle < 0)
            goto fail;
#ifdef HAVE_STRUCT_LINGER_L_LINGER
        li.l_onoff = 1;
        li.l_linger = 120;
#endif
        setsockopt (fh->handle, SOL_SOCKET, SO_LINGER, &li, sizeof (li));

        if (fh->ino->localname)
        {
            unlink (fh->ino->localname);
            g_free (fh->ino->localname);
            fh->ino->localname = NULL;
        }
        return 0;
    }

    if (!fh->ino->localname && vfs_s_retrieve_file (me, fh->ino) == -1)
        goto fail;
    if (!fh->ino->localname)
        vfs_die ("retrieve_file failed to fill in localname");
    return 0;

  fail:
    ftpfs_fh_free_data (fh);
    return -1;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_fh_close (struct vfs_class *me, vfs_file_handler_t * fh)
{
    if (fh->handle != -1 && !fh->ino->localname)
    {
        ftp_super_data_t *ftp = (ftp_super_data_t *) fh->ino->super->data;

        close (fh->handle);
        fh->handle = -1;
        /* File is stored to destination already, so
         * we prevent MEDATA->ftpfs_file_store() call from vfs_s_close ()
         */
        fh->changed = 0;
        if (ftpfs_get_reply (me, ftp->sock, NULL, 0) != COMPLETE)
            ERRNOR (EIO, -1);
        vfs_s_invalidate (me, FH_SUPER);
    }

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_done (struct vfs_class *me)
{
    (void) me;

    g_slist_foreach (no_proxy, (GFunc) g_free, NULL);
    g_slist_free (no_proxy);

    g_free (ftpfs_anonymous_passwd);
    g_free (ftpfs_proxy_host);
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_fill_names (struct vfs_class *me, fill_names_f func)
{
    GList *iter;

    for (iter = MEDATA->supers; iter != NULL; iter = g_list_next (iter))
    {
        const struct vfs_s_super *super = (const struct vfs_s_super *) iter->data;
        char *name;

        name = vfs_path_element_build_pretty_path_str (super->path_element);

        func (name);
        g_free (name);
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

    while (1)
    {
        netrcp = skip_separators (netrcp);
        if (*netrcp != '\n')
            break;
        netrcp++;
    }
    if (!*netrcp)
        return NETRC_NONE;
    p = buffer;
    if (*netrcp == '"')
    {
        for (netrcp++; *netrcp != '"' && *netrcp; netrcp++)
        {
            if (*netrcp == '\\')
                netrcp++;
            *p++ = *netrcp;
        }
    }
    else
    {
        for (; *netrcp != '\n' && *netrcp != '\t' && *netrcp != ' ' &&
             *netrcp != ',' && *netrcp; netrcp++)
        {
            if (*netrcp == '\\')
                netrcp++;
            *p++ = *netrcp;
        }
    }
    *p = 0;
    if (!*buffer)
        return NETRC_NONE;

    for (i = NETRC_DEFAULT; keywords[i - 1] != NULL; i++)
        if (strcmp (keywords[i - 1], buffer) == 0)
            return i;

    return NETRC_UNKNOWN;
}

/* --------------------------------------------------------------------------------------------- */

static int
ftpfs_netrc_bad_mode (const char *netrcname)
{
    static int be_angry = 1;
    struct stat mystat;

    if (stat (netrcname, &mystat) >= 0 && (mystat.st_mode & 077))
    {
        if (be_angry)
        {
            message (D_ERROR, MSG_ERROR,
                     _("~/.netrc file has incorrect mode\nRemove password or correct mode"));
            be_angry = 0;
        }
        return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/* Scan .netrc until we find matching "machine" or "default"
 * domain is used for additional matching
 * No search is done after "default" in compliance with "man netrc"
 * Return 0 if found, -1 otherwise */

static int
ftpfs_find_machine (const char *host, const char *domain)
{
    keyword_t keyword;

    if (!host)
        host = "";
    if (!domain)
        domain = "";

    while ((keyword = ftpfs_netrc_next ()) != NETRC_NONE)
    {
        if (keyword == NETRC_DEFAULT)
            return 0;

        if (keyword == NETRC_MACDEF)
        {
            /* Scan for an empty line, which concludes "macdef" */
            do
            {
                while (*netrcp && *netrcp != '\n')
                    netrcp++;
                if (*netrcp != '\n')
                    break;
                netrcp++;
            }
            while (*netrcp && *netrcp != '\n');
            continue;
        }

        if (keyword != NETRC_MACHINE)
            continue;

        /* Take machine name */
        if (ftpfs_netrc_next () == NETRC_NONE)
            break;

        if (g_ascii_strcasecmp (host, buffer) != 0)
        {
            /* Try adding our domain to short names in .netrc */
            const char *host_domain = strchr (host, '.');
            if (!host_domain)
                continue;

            /* Compare domain part */
            if (g_ascii_strcasecmp (host_domain, domain) != 0)
                continue;

            /* Compare local part */
            if (g_ascii_strncasecmp (host, buffer, host_domain - host) != 0)
                continue;
        }

        return 0;
    }

    /* end of .netrc */
    return -1;
}

/* --------------------------------------------------------------------------------------------- */
/* Extract login and password from .netrc for the host.
 * pass may be NULL.
 * Returns 0 for success, -1 for error */

static int
ftpfs_netrc_lookup (const char *host, char **login, char **pass)
{
    char *netrcname;
    char *tmp_pass = NULL;
    char hostname[MAXHOSTNAMELEN];
    const char *domain;
    keyword_t keyword;
    static struct rupcache
    {
        struct rupcache *next;
        char *host;
        char *login;
        char *pass;
    } *rup_cache = NULL, *rupp;

    /* Initialize *login and *pass */
    g_free (*login);
    *login = NULL;
    g_free (*pass);
    *pass = NULL;

    /* Look up in the cache first */
    for (rupp = rup_cache; rupp != NULL; rupp = rupp->next)
    {
        if (!strcmp (host, rupp->host))
        {
            if (rupp->login)
                *login = g_strdup (rupp->login);
            if (pass && rupp->pass)
                *pass = g_strdup (rupp->pass);
            return 0;
        }
    }

    /* Load current .netrc */
    netrcname = g_build_filename (mc_config_get_home_dir (), ".netrc", (char *) NULL);
    if (!g_file_get_contents (netrcname, &netrc, NULL, NULL))
    {
        g_free (netrcname);
        return 0;
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
    while (1)
    {
        int need_break = 0;
        keyword = ftpfs_netrc_next ();

        switch (keyword)
        {
        case NETRC_LOGIN:
            if (ftpfs_netrc_next () == NETRC_NONE)
            {
                need_break = 1;
                break;
            }

            /* We have another name already - should not happen */
            if (*login)
            {
                need_break = 1;
                break;
            }

            /* We have login name now */
            *login = g_strdup (buffer);
            break;

        case NETRC_PASSWORD:
        case NETRC_PASSWD:
            if (ftpfs_netrc_next () == NETRC_NONE)
            {
                need_break = 1;
                break;
            }

            /* Ignore unsafe passwords */
            if (*login != NULL &&
                strcmp (*login, "anonymous") != 0 && strcmp (*login, "ftp") != 0
                && ftpfs_netrc_bad_mode (netrcname))
            {
                need_break = 1;
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
                need_break = 1;
                break;
            }

            /* Ignore account, but warn user anyways */
            ftpfs_netrc_bad_mode (netrcname);
            break;

        default:
            /* Unexpected keyword or end of file */
            need_break = 1;
            break;
        }

        if (need_break)
            break;
    }

    g_free (netrc);
    g_free (netrcname);

    rupp = g_new (struct rupcache, 1);
    rupp->host = g_strdup (host);
    rupp->login = g_strdup (*login);
    rupp->pass = g_strdup (tmp_pass);

    rupp->next = rup_cache;
    rup_cache = rupp;

    *pass = tmp_pass;

    return 0;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/** This routine is called as the last step in load_setup */
void
ftpfs_init_passwd (void)
{
    ftpfs_anonymous_passwd = load_anon_passwd ();
    if (ftpfs_anonymous_passwd)
        return;

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

/* --------------------------------------------------------------------------------------------- */

void
init_ftpfs (void)
{
    static struct vfs_s_subclass ftpfs_subclass;

    tcp_init ();

    ftpfs_subclass.flags = VFS_S_REMOTE | VFS_S_USETMP;
    ftpfs_subclass.archive_same = ftpfs_archive_same;
    ftpfs_subclass.open_archive = ftpfs_open_archive;
    ftpfs_subclass.free_archive = ftpfs_free_archive;
    ftpfs_subclass.fh_open = ftpfs_fh_open;
    ftpfs_subclass.fh_close = ftpfs_fh_close;
    ftpfs_subclass.fh_free_data = ftpfs_fh_free_data;
    ftpfs_subclass.dir_load = ftpfs_dir_load;
    ftpfs_subclass.file_store = ftpfs_file_store;
    ftpfs_subclass.linear_start = ftpfs_linear_start;
    ftpfs_subclass.linear_read = ftpfs_linear_read;
    ftpfs_subclass.linear_close = ftpfs_linear_close;

    vfs_s_init_class (&vfs_ftpfs_ops, &ftpfs_subclass);
    vfs_ftpfs_ops.name = "ftpfs";
    vfs_ftpfs_ops.flags = VFSF_NOLINKS;
    vfs_ftpfs_ops.prefix = "ftp";
    vfs_ftpfs_ops.done = &ftpfs_done;
    vfs_ftpfs_ops.fill_names = ftpfs_fill_names;
    vfs_ftpfs_ops.chmod = ftpfs_chmod;
    vfs_ftpfs_ops.chown = ftpfs_chown;
    vfs_ftpfs_ops.unlink = ftpfs_unlink;
    vfs_ftpfs_ops.rename = ftpfs_rename;
    vfs_ftpfs_ops.mkdir = ftpfs_mkdir;
    vfs_ftpfs_ops.rmdir = ftpfs_rmdir;
    vfs_ftpfs_ops.ctl = ftpfs_ctl;
    vfs_register_class (&vfs_ftpfs_ops);
}

/* --------------------------------------------------------------------------------------------- */
