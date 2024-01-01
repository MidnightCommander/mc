/*
   Virtual File System: FTP file system

   Copyright (C) 2015-2024
   The Free Software Foundation, Inc.

   Written by: Andrew Borodin <aborodin@vmail.ru>, 2013

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
 *  \brief Source: Virtual File System: FTP file system
 *  \author Andrew Borodin
 *  \date 2015
 *
 *  Parser of ftp long file list (reply to "LIST -la" command).
 *  Borrowed from lftp project (http://http://lftp.yar.ru/).
 *  Author of original lftp code: Alexander V. Lukyanov (lav@yars.free.net)
 */

#include <config.h>

#include <ctype.h>              /* isdigit() */
#include <stdio.h>              /* sscanf() */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>           /* mode_t */
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "lib/global.h"

#include "lib/vfs/vfs.h"
#include "lib/vfs/utilvfs.h"

#include "ftpfs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define number_of_parsers 7

#define NO_SIZE     ((off_t) (-1L))
#define NO_DATE     ((time_t) (-1L))

#define FIRST_TOKEN strtok (line, " \t")
#define NEXT_TOKEN  strtok (NULL, " \t")
#define FIRST_TOKEN_R strtok_r (line, " \t", &next)
#define NEXT_TOKEN_R  strtok_r (NULL, " \t", &next)

#define ERR2 do { (*err)++; return FALSE; } while (FALSE)

/*** file scope type declarations ****************************************************************/

typedef enum
{
    UNKNOWN = 0,
    DIRECTORY,
    SYMLINK,
    NORMAL
} filetype;

typedef gboolean (*ftpfs_line_parser) (char *line, struct stat * s, char **filename,
                                       char **linkname, int *err);

/*** forward declarations (file scope functions) *************************************************/

static gboolean ftpfs_parse_long_list_UNIX (char *line, struct stat *s, char **filename,
                                            char **linkname, int *err);
static gboolean ftpfs_parse_long_list_NT (char *line, struct stat *s, char **filename,
                                          char **linkname, int *err);
static gboolean ftpfs_parse_long_list_EPLF (char *line, struct stat *s, char **filename,
                                            char **linkname, int *err);
static gboolean ftpfs_parse_long_list_MLSD (char *line, struct stat *s, char **filename,
                                            char **linkname, int *err);
static gboolean ftpfs_parse_long_list_AS400 (char *line, struct stat *s, char **filename,
                                             char **linkname, int *err);
static gboolean ftpfs_parse_long_list_OS2 (char *line, struct stat *s, char **filename,
                                           char **linkname, int *err);
static gboolean ftpfs_parse_long_list_MacWebStar (char *line, struct stat *s, char **filename,
                                                  char **linkname, int *err);

/*** file scope variables ************************************************************************/

static time_t rawnow;
static struct tm now;

static ftpfs_line_parser line_parsers[number_of_parsers] = {
    ftpfs_parse_long_list_UNIX,
    ftpfs_parse_long_list_NT,
    ftpfs_parse_long_list_EPLF,
    ftpfs_parse_long_list_MLSD,
    ftpfs_parse_long_list_AS400,
    ftpfs_parse_long_list_OS2,
    ftpfs_parse_long_list_MacWebStar
};

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline uid_t
ftpfs_get_uid (const char *s)
{
    uid_t u;

    if (*s < '0' || *s > '9')
        u = vfs_finduid (s);
    else
        u = (uid_t) atol (s);

    return u;
}

/* --------------------------------------------------------------------------------------------- */

static inline gid_t
ftpfs_get_gid (const char *s)
{
    gid_t g;

    if (*s < '0' || *s > '9')
        g = vfs_findgid (s);
    else
        g = (gid_t) atol (s);

    return g;
}

/* --------------------------------------------------------------------------------------------- */

static void
ftpfs_init_time (void)
{
    time (&rawnow);
    now = *localtime (&rawnow);
}

/* --------------------------------------------------------------------------------------------- */

static int
guess_year (int month, int day, int hour, int minute)
{
    int year;

    (void) hour;
    (void) minute;

    year = now.tm_year + 1900;

    if (month * 32 + day > now.tm_mon * 32 + now.tm_mday + 6)
        year--;

    return year;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
parse_year_or_time (const char *year_or_time, int *year, int *hour, int *minute)
{
    if (year_or_time[2] == ':')
    {
        if (sscanf (year_or_time, "%2d:%2d", hour, minute) != 2)
            return FALSE;

        *year = -1;
    }
    else
    {
        if (sscanf (year_or_time, "%d", year) != 1)
            return FALSE;

        *hour = *minute = 0;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/* Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone (mktime assumes the latter).

   Contributed by Roger Beeman <beeman@cisco.com>, with the help of
   Mark Baushke <mdb@cisco.com> and the rest of the Gurus at CISCO.  */
static time_t
mktime_from_utc (const struct tm *t)
{
    struct tm tc;
    time_t tl, tb;

    memcpy (&tc, t, sizeof (struct tm));

    /* UTC times are never DST; if we say -1, we'll introduce odd localtime-
     * dependent errors. */

    tc.tm_isdst = 0;

    tl = mktime (&tc);
    if (tl == -1)
        return (-1);

    tb = mktime (gmtime (&tl));

    return (tl <= tb ? (tl + (tl - tb)) : (tl - (tb - tl)));
}

/* --------------------------------------------------------------------------------------------- */

static time_t
ftpfs_convert_date (const char *s)
{
    struct tm tm;
    int year, month, day, hour, minute, second;
    int skip = 0;
    int n;

    memset (&tm, 0, sizeof (tm));

    n = sscanf (s, "%4d%n", &year, &skip);

    /* try to workaround server's y2k bug *
     * I hope in the next 300 years the y2k bug will be finally fixed :) */
    if (n == 1 && year >= 1910 && year <= 1930)
    {
        n = sscanf (s, "%5d%n", &year, &skip);
        year = year - 19100 + 2000;
    }

    if (n != 1)
        return NO_DATE;

    n = sscanf (s + skip, "%2d%2d%2d%2d%2d", &month, &day, &hour, &minute, &second);

    if (n != 5)
        return NO_DATE;

    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

    return mktime_from_utc (&tm);
}

/* --------------------------------------------------------------------------------------------- */

/*
   -rwxr-xr-x   1 lav      root         4771 Sep 12  1996 install-sh
   -rw-r--r--   1 lav      root         1349 Feb  2 14:10 lftp.lsm
   drwxr-xr-x   4 lav      root         1024 Feb 22 15:32 lib
   lrwxrwxrwx   1 lav      root           33 Feb 14 17:45 ltconfig -> /usr/share/libtool/ltconfig

   NOTE: group may be missing.
 */

static gboolean
parse_ls_line (char *line, struct stat *s, char **filename, char **linkname)
{
    char *next = NULL;
    char *t;
    mode_t type, mode = 0;
    char *group_or_size;
    struct tm date;
    const char *day_of_month;
    gboolean year_anomaly = FALSE;
    char *name;

    /* parse perms */
    t = FIRST_TOKEN_R;
    if (t == NULL)
        return FALSE;

    if (!vfs_parse_filetype (t, NULL, &type))
        return FALSE;

    if (vfs_parse_fileperms (t + 1, NULL, &mode))
        mode |= type;

    s->st_mode = mode;

    /* link count */
    t = NEXT_TOKEN_R;
    if (t == NULL)
        return FALSE;
    s->st_nlink = atol (t);

    /* user */
    t = NEXT_TOKEN_R;
    if (t == NULL)
        return FALSE;

    s->st_uid = ftpfs_get_uid (t);

    /* group or size */
    group_or_size = NEXT_TOKEN_R;

    /* size or month */
    t = NEXT_TOKEN_R;
    if (t == NULL)
        return FALSE;
    if (isdigit ((unsigned char) *t))
    {
        /* it's size, so the previous was group: */
        long long size;
        int n;

        s->st_gid = ftpfs_get_gid (group_or_size);

        if (sscanf (t, "%lld%n", &size, &n) == 1 && t[n] == '\0')
            s->st_size = (off_t) size;
        t = NEXT_TOKEN_R;
        if (t == NULL)
            return FALSE;
    }
    else
    {
        /*  it was month, so the previous was size: */
        long long size;
        int n;

        if (sscanf (group_or_size, "%lld%n", &size, &n) == 1 && group_or_size[n] == '\0')
            s->st_size = (off_t) size;
    }

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    s->st_blksize = 512;
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    s->st_blocks = (s->st_size + 511) / 512;
#endif

    memset (&date, 0, sizeof (date));

    if (!vfs_parse_month (t, &date))
        date.tm_mon = 0;

    day_of_month = NEXT_TOKEN_R;
    if (day_of_month == NULL)
        return FALSE;
    date.tm_mday = atoi (day_of_month);

    /* time or year */
    t = NEXT_TOKEN_R;
    if (t == NULL)
        return FALSE;
    date.tm_isdst = -1;
    date.tm_hour = date.tm_min = 0;
    date.tm_sec = 30;

    if (sscanf (t, "%2d:%2d", &date.tm_hour, &date.tm_min) == 2)
        date.tm_year = guess_year (date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min) - 1900;
    else
    {
        if (day_of_month + strlen (day_of_month) + 1 == t)
            year_anomaly = TRUE;
        date.tm_year = atoi (t) - 1900;
        /* We don't know the hour.  Set it to something other than 0, or
         * DST -1 will end up changing the date. */
        date.tm_hour = 12;
        date.tm_min = 0;
        date.tm_sec = 0;
    }

    s->st_mtime = mktime (&date);
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;

    name = strtok_r (NULL, "", &next);
    if (name == NULL)
        return FALSE;

    /* there are ls which output extra space after year. */
    if (year_anomaly && *name == ' ')
        name++;

    if (!S_ISLNK (s->st_mode))
        *linkname = NULL;
    else
    {
        char *arrow;

        for (arrow = name; (arrow = strstr (arrow, " -> ")) != NULL; arrow++)
            if (arrow != name && arrow[4] != '\0')
            {
                *arrow = '\0';
                *linkname = g_strdup (arrow + 4);
                break;
            }
    }

    *filename = g_strdup (name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftpfs_parse_long_list_UNIX (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    int tmp;
    gboolean ret;

    if (sscanf (line, "total %d", &tmp) == 1)
        return FALSE;

    if (strncasecmp (line, "Status of ", 10) == 0)
        return FALSE;           /* STAT output. */

    ret = parse_ls_line (line, s, filename, linkname);
    if (!ret)
        (*err)++;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

/*
   07-13-98  09:06PM       <DIR>          aix
   07-13-98  09:06PM       <DIR>          hpux
   07-13-98  09:06PM       <DIR>          linux
   07-13-98  09:06PM       <DIR>          ncr
   07-13-98  09:06PM       <DIR>          solaris
   03-18-98  06:01AM              2109440 nlxb318e.tar
   07-02-98  11:17AM                13844 Whatsnew.txt
 */

static gboolean
ftpfs_parse_long_list_NT (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    char *t;
    int month, day, year, hour, minute;
    char am;
    struct tm tms;
    long long size;

    t = FIRST_TOKEN;
    if (t == NULL)
        ERR2;
    if (sscanf (t, "%2d-%2d-%2d", &month, &day, &year) != 3)
        ERR2;
    if (year >= 70)
        year += 1900;
    else
        year += 2000;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    am = 'A';                   /* AM/PM is optional */
    if (sscanf (t, "%2d:%2d%c", &hour, &minute, &am) < 2)
        ERR2;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;

    if (am == 'P')              /* PM - after noon */
    {
        hour += 12;
        if (hour == 24)
            hour = 0;
    }

    tms.tm_sec = 30;            /* seconds after the minute [0, 61]  */
    tms.tm_min = minute;        /* minutes after the hour [0, 59] */
    tms.tm_hour = hour;         /* hour since midnight [0, 23] */
    tms.tm_mday = day;          /* day of the month [1, 31] */
    tms.tm_mon = month - 1;     /* months since January [0, 11] */
    tms.tm_year = year - 1900;  /* years since 1900 */
    tms.tm_isdst = -1;


    s->st_mtime = mktime (&tms);
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;

    if (strcmp (t, "<DIR>") == 0)
        s->st_mode = S_IFDIR;
    else
    {
        s->st_mode = S_IFREG;
        if (sscanf (t, "%lld", &size) != 1)
            ERR2;
        s->st_size = (off_t) size;
    }

    t = strtok (NULL, "");
    if (t == NULL)
        ERR2;
    while (*t == ' ')
        t++;
    if (*t == '\0')
        ERR2;

    *filename = g_strdup (t);
    *linkname = NULL;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*
   +i774.71425,m951188401,/,       users
   +i774.49602,m917883130,r,s79126,        jgr_www2.exe

   starts with +
   comma separated
   first character of field is type:
   i - ?
   m - modification time
   / - means directory
   r - means plain file
   s - size
   up - permissions in octal
   \t - file name follows.
 */

static gboolean
ftpfs_parse_long_list_EPLF (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    size_t len;
    const char *b;
    const char *name = NULL;
    size_t name_len = 0;
    off_t size = NO_SIZE;
    time_t date = NO_DATE;
    long date_l;
    long long size_ll;
    gboolean dir = FALSE;
    gboolean type_known = FALSE;
    int perms = -1;
    const char *scan;
    ssize_t scan_len;

    len = strlen (line);
    b = line;

    if (len < 2 || b[0] != '+')
        ERR2;

    scan = b + 1;
    scan_len = len - 1;

    while (scan != NULL && scan_len > 0)
    {
        const char *comma;

        switch (*scan)
        {
        case '\t':             /* the rest is file name. */
            name = scan + 1;
            name_len = scan_len - 1;
            scan = NULL;
            break;
        case 's':
            if (sscanf (scan + 1, "%lld", &size_ll) != 1)
                break;
            size = size_ll;
            break;
        case 'm':
            if (sscanf (scan + 1, "%ld", &date_l) != 1)
                break;
            date = date_l;
            break;
        case '/':
            dir = TRUE;
            type_known = TRUE;
            break;
        case 'r':
            dir = FALSE;
            type_known = TRUE;
            break;
        case 'i':
            break;
        case 'u':
            if (scan[1] == 'p') /* permissions. */
                if (sscanf (scan + 2, "%o", (unsigned int *) &perms) != 1)
                    perms = -1;
            break;
        default:
            name = NULL;
            scan = NULL;
            break;
        }
        if (scan == NULL || scan_len == 0)
            break;

        comma = (const char *) memchr (scan, ',', scan_len);
        if (comma == NULL)
            break;

        scan_len -= comma + 1 - scan;
        scan = comma + 1;
    }

    if (name == NULL || !type_known)
        ERR2;

    *filename = g_strndup (name, name_len);
    *linkname = NULL;

    if (size != NO_SIZE)
        s->st_size = size;
    if (date != NO_DATE)
    {
        s->st_mtime = date;
        /* Use resulting time value */
        s->st_atime = s->st_ctime = s->st_mtime;
    }
    if (type_known)
        s->st_mode = dir ? S_IFDIR : S_IFREG;
    if (perms != -1)
        s->st_mode |= perms;    /* FIXME */

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*
   Type=cdir;Modify=20021029173810;Perm=el;Unique=BP8AAjJufAA; /
   Type=pdir;Modify=20021029173810;Perm=el;Unique=BP8AAjJufAA; ..
   Type=dir;Modify=20010118144705;Perm=e;Unique=BP8AAjNufAA; bin
   Type=dir;Modify=19981021003019;Perm=el;Unique=BP8AAlhufAA; pub
   Type=file;Size=12303;Modify=19970124132601;Perm=r;Unique=BP8AAo9ufAA; mailserv.FAQ
   modify=20161215062118;perm=flcdmpe;type=dir;UNIX.group=503;UNIX.mode=0700; directory-name
   modify=20161213121618;perm=adfrw;size=6369064;type=file;UNIX.group=503;UNIX.mode=0644; file-name
   modify=20120103123744;perm=adfrw;size=11;type=OS.unix=symlink;UNIX.group=0;UNIX.mode=0777; www
 */

static gboolean
ftpfs_parse_long_list_MLSD (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    const char *name = NULL;
    off_t size = NO_SIZE;
    time_t date = NO_DATE;
    const char *owner = NULL;
    const char *group = NULL;
    filetype type = UNKNOWN;
    int perms = -1;
    char *space;
    char *tok;

    space = strstr (line, "; ");
    if (space != NULL)
    {
        name = space + 2;
        *space = '\0';
    }
    else
    {
        /* NcFTPd does not put a semicolon after last fact, workaround it. */
        space = strchr (line, ' ');
        if (space == NULL)
            ERR2;
        name = space + 1;
        *space = '\0';
    }

    for (tok = strtok (line, ";"); tok != NULL; tok = strtok (NULL, ";"))
    {
        if (strcasecmp (tok, "Type=cdir") == 0
            || strcasecmp (tok, "Type=pdir") == 0 || strcasecmp (tok, "Type=dir") == 0)
        {
            type = DIRECTORY;
            continue;
        }
        if (strcasecmp (tok, "Type=file") == 0)
        {
            type = NORMAL;
            continue;
        }
        if (strcasecmp (tok, "Type=OS.unix=symlink") == 0)
        {
            type = SYMLINK;
            continue;
        }
        if (strncasecmp (tok, "Modify=", 7) == 0)
        {
            date = ftpfs_convert_date (tok + 7);
            continue;
        }
        if (strncasecmp (tok, "Size=", 5) == 0)
        {
            long long size_ll;

            if (sscanf (tok + 5, "%lld", &size_ll) == 1)
                size = size_ll;
            continue;
        }
        if (strncasecmp (tok, "Perm=", 5) == 0)
        {
            perms = 0;
            for (tok += 5; *tok != '\0'; tok++)
            {
                switch (g_ascii_tolower (*tok))
                {
                case 'e':
                    perms |= 0111;
                    break;
                case 'l':
                    perms |= 0444;
                    break;
                case 'r':
                    perms |= 0444;
                    break;
                case 'c':
                    perms |= 0200;
                    break;
                case 'w':
                    perms |= 0200;
                    break;
                default:
                    break;
                }
            }
            continue;
        }
        if (strncasecmp (tok, "UNIX.mode=", 10) == 0)
        {
            if (sscanf (tok + 10, "%o", (unsigned int *) &perms) != 1)
                perms = -1;
            continue;
        }
        if (strncasecmp (tok, "UNIX.owner=", 11) == 0)
        {
            owner = tok + 11;
            continue;
        }
        if (strncasecmp (tok, "UNIX.group=", 11) == 0)
        {
            group = tok + 11;
            continue;
        }
        if (strncasecmp (tok, "UNIX.uid=", 9) == 0)
        {
            if (owner == NULL)
                owner = tok + 9;
            continue;
        }
        if (strncasecmp (tok, "UNIX.gid=", 9) == 0)
        {
            if (group == NULL)
                group = tok + 9;
            continue;
        }
    }
    if (name == NULL || name[0] == '\0' || type == UNKNOWN)
        ERR2;

    *filename = g_strdup (name);
    *linkname = NULL;

    if (size != NO_SIZE)
        s->st_size = size;
    if (date != NO_DATE)
    {
        s->st_mtime = date;
        /* Use resulting time value */
        s->st_atime = s->st_ctime = s->st_mtime;
    }
    switch (type)
    {
    case DIRECTORY:
        s->st_mode = S_IFDIR;
        break;
    case SYMLINK:
        s->st_mode = S_IFLNK;
        break;
    case NORMAL:
        s->st_mode = S_IFREG;
        break;
    default:
        g_assert_not_reached ();
    }
    if (perms != -1)
        s->st_mode |= perms;    /* FIXME */
    if (owner != NULL)
        s->st_uid = ftpfs_get_uid (owner);
    if (group != NULL)
        s->st_gid = ftpfs_get_gid (group);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*
   ASUSER          8192 04/26/05 13:54:16 *DIR       dir/
   ASUSER          8192 04/26/05 13:57:34 *DIR       dir1/
   ASUSER        365255 02/28/01 15:41:40 *STMF      readme.txt
   ASUSER       8489625 03/18/03 09:37:00 *STMF      saved.zip
   ASUSER        365255 02/28/01 15:41:40 *STMF      unist.old
 */

static gboolean
ftpfs_parse_long_list_AS400 (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    char *t;
    char *user;
    long long size;
    int month, day, year, hour, minute, second;
    struct tm tms;
    time_t mtime;
    mode_t type;
    char *slash;

    t = FIRST_TOKEN;
    if (t == NULL)
        ERR2;
    user = t;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (sscanf (t, "%lld", &size) != 1)
        ERR2;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (sscanf (t, "%2d/%2d/%2d", &month, &day, &year) != 3)
        ERR2;
    if (year >= 70)
        year += 1900;
    else
        year += 2000;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (sscanf (t, "%2d:%2d:%2d", &hour, &minute, &second) != 3)
        ERR2;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;

    tms.tm_sec = second;        /* seconds after the minute [0, 61]  */
    tms.tm_min = minute;        /* minutes after the hour [0, 59] */
    tms.tm_hour = hour;         /* hour since midnight [0, 23] */
    tms.tm_mday = day;          /* day of the month [1, 31] */
    tms.tm_mon = month - 1;     /* months since January [0, 11] */
    tms.tm_year = year - 1900;  /* years since 1900 */
    tms.tm_isdst = -1;
    mtime = mktime (&tms);

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (strcmp (t, "*DIR") == 0)
        type = S_IFDIR;
    else
        type = S_IFREG;

    t = strtok (NULL, "");
    if (t == NULL)
        ERR2;
    while (*t == ' ')
        t++;
    if (*t == '\0')
        ERR2;

    *linkname = NULL;

    slash = strchr (t, '/');
    if (slash != NULL)
    {
        if (slash == t)
            return FALSE;

        *slash = '\0';
        type = S_IFDIR;
        if (slash[1] != '\0')
        {
            *filename = g_strdup (t);
            s->st_mode = type;  /* FIXME */
            return TRUE;
        }
    }

    *filename = g_strdup (t);
    s->st_mode = type;
    s->st_size = (off_t) size;
    s->st_mtime = mtime;
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;
    s->st_uid = ftpfs_get_uid (user);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

/*
   0          DIR  06-27-96  11:57  PROTOCOL
   169               11-29-94  09:20  SYSLEVEL.MPT
 */

static gboolean
ftpfs_parse_long_list_OS2 (char *line, struct stat *s, char **filename, char **linkname, int *err)
{
    char *t;
    long long size;
    int month, day, year, hour, minute;
    struct tm tms;

    t = FIRST_TOKEN;
    if (t == NULL)
        ERR2;

    if (sscanf (t, "%lld", &size) != 1)
        ERR2;
    s->st_size = (off_t) size;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    s->st_mode = S_IFREG;
    if (strcmp (t, "DIR") == 0)
    {
        s->st_mode = S_IFDIR;
        t = NEXT_TOKEN;

        if (t == NULL)
            ERR2;
    }

    if (sscanf (t, "%2d-%2d-%2d", &month, &day, &year) != 3)
        ERR2;
    if (year >= 70)
        year += 1900;
    else
        year += 2000;

    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (sscanf (t, "%2d:%2d", &hour, &minute) != 3)
        ERR2;

    tms.tm_sec = 30;            /* seconds after the minute [0, 61]  */
    tms.tm_min = minute;        /* minutes after the hour [0, 59] */
    tms.tm_hour = hour;         /* hour since midnight [0, 23] */
    tms.tm_mday = day;          /* day of the month [1, 31] */
    tms.tm_mon = month - 1;     /* months since January [0, 11] */
    tms.tm_year = year - 1900;  /* years since 1900 */
    tms.tm_isdst = -1;
    s->st_mtime = mktime (&tms);
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;

    t = strtok (NULL, "");
    if (t == NULL)
        ERR2;
    while (*t == ' ')
        t++;
    if (*t == '\0')
        ERR2;
    *filename = g_strdup (t);
    *linkname = NULL;

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
ftpfs_parse_long_list_MacWebStar (char *line, struct stat *s, char **filename,
                                  char **linkname, int *err)
{
    char *t;
    mode_t type, mode;
    struct tm date;
    const char *day_of_month;
    char *name;

    t = FIRST_TOKEN;
    if (t == NULL)
        ERR2;

    if (!vfs_parse_filetype (t, NULL, &type))
        ERR2;

    s->st_mode = type;

    if (!vfs_parse_fileperms (t + 1, NULL, &mode))
        ERR2;
    /* permissions are meaningless here. */

    /* "folder" or 0 */
    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;

    if (strcmp (t, "folder") != 0)
    {
        long long size;

        /* size? */
        t = NEXT_TOKEN;
        if (t == NULL)
            ERR2;
        /* size */
        t = NEXT_TOKEN;
        if (t == NULL)
            ERR2;
        if (!isdigit ((unsigned char) *t))
            ERR2;

        if (sscanf (t, "%lld", &size) == 1)
            s->st_size = (off_t) size;
    }
    else
    {
        /* ?? */
        t = NEXT_TOKEN;
        if (t == NULL)
            ERR2;
    }

    /* month */
    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;

    memset (&date, 0, sizeof (date));

    if (!vfs_parse_month (t, &date))
        ERR2;

    day_of_month = NEXT_TOKEN;
    if (day_of_month == NULL)
        ERR2;

    date.tm_mday = atoi (day_of_month);

    /* time or year */
    t = NEXT_TOKEN;
    if (t == NULL)
        ERR2;
    if (!parse_year_or_time (t, &date.tm_year, &date.tm_hour, &date.tm_min))
        ERR2;

    date.tm_isdst = -1;
    date.tm_sec = 30;
    if (date.tm_year == -1)
        date.tm_year = guess_year (date.tm_mon, date.tm_mday, date.tm_hour, date.tm_min) - 1900;
    else
        date.tm_hour = 12;

    s->st_mtime = mktime (&date);
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;

    name = strtok (NULL, "");
    if (name == NULL)
        ERR2;

    /* no symlinks on Mac, but anyway. */
    if (!S_ISLNK (s->st_mode))
        *linkname = NULL;
    else
    {
        char *arrow;

        for (arrow = name; (arrow = strstr (arrow, " -> ")) != NULL; arrow++)
            if (arrow != name && arrow[4] != '\0')
            {
                *arrow = '\0';
                *linkname = g_strdup (arrow + 4);
                break;
            }
    }

    *filename = g_strdup (name);

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

GSList *
ftpfs_parse_long_list (struct vfs_class * me, struct vfs_s_inode * dir, GSList * buf, int *err_ret)
{
    int err[number_of_parsers];
    GSList *set[number_of_parsers];     /* arrays of struct vfs_s_entry */
    size_t i;
    GSList *bufp;
    ftpfs_line_parser guessed_parser = NULL;
    GSList **the_set = NULL;
    int *the_err = NULL;
    int *best_err1 = &err[0];
    int *best_err2 = &err[1];

    ftpfs_init_time ();

    if (err_ret != NULL)
        *err_ret = 0;

    memset (&err, 0, sizeof (err));
    memset (&set, 0, sizeof (set));

    for (bufp = buf; bufp != NULL; bufp = g_slist_next (bufp))
    {
        char *b = (char *) bufp->data;
        size_t blen;

        blen = strlen (b);

        if (b[blen - 1] == '\r')
        {
            b[blen - 1] = '\0';
            blen--;
        }

        if (blen == 0)
            continue;

        if (guessed_parser == NULL)
        {
            for (i = 0; i < number_of_parsers; i++)
            {
                struct vfs_s_entry *info;
                gboolean ok;
                char *tmp_line;
                int nlink;

                /* parser can clobber the line - work on a copy */
                tmp_line = g_strndup (b, blen);

                info = vfs_s_generate_entry (me, NULL, dir, 0);
                nlink = info->ino->st.st_nlink;
                ok = (*line_parsers[i]) (tmp_line, &info->ino->st, &info->name,
                                         &info->ino->linkname, &err[i]);
                if (ok && strchr (info->name, '/') == NULL)
                {
                    info->ino->st.st_nlink = nlink;     /* Ouch, we need to preserve our counts :-( */
                    set[i] = g_slist_prepend (set[i], info);
                }
                else
                    vfs_s_free_entry (me, info);

                g_free (tmp_line);

                if (*best_err1 > err[i])
                    best_err1 = &err[i];
                if (*best_err2 > err[i] && best_err1 != &err[i])
                    best_err2 = &err[i];

                if (*best_err1 > 16)
                    goto leave; /* too many errors with best parser. */
            }

            if (*best_err2 > (*best_err1 + 1) * 16)
            {
                i = (size_t) (best_err1 - err);
                guessed_parser = line_parsers[i];
                the_set = &set[i];
                the_err = &err[i];
            }
        }
        else
        {
            struct vfs_s_entry *info;
            gboolean ok;
            char *tmp_line;
            int nlink;

            /* parser can clobber the line - work on a copy */
            tmp_line = g_strndup (b, blen);

            info = vfs_s_generate_entry (me, NULL, dir, 0);
            nlink = info->ino->st.st_nlink;
            ok = guessed_parser (tmp_line, &info->ino->st, &info->name, &info->ino->linkname,
                                 the_err);
            if (ok && strchr (info->name, '/') == NULL)
            {
                info->ino->st.st_nlink = nlink; /* Ouch, we need to preserve our counts :-( */
                *the_set = g_slist_prepend (*the_set, info);
            }
            else
                vfs_s_free_entry (me, info);

            g_free (tmp_line);
        }
    }

    if (the_set == NULL)
    {
        i = best_err1 - err;
        the_set = &set[i];
        the_err = &err[i];
    }

  leave:
    for (i = 0; i < number_of_parsers; i++)
        if (&set[i] != the_set)
        {
            for (bufp = set[i]; bufp != NULL; bufp = g_slist_next (bufp))
                vfs_s_free_entry (me, VFS_ENTRY (bufp->data));

            g_slist_free (set[i]);
        }

    if (err_ret != NULL && the_err != NULL)
        *err_ret = *the_err;

    return the_set != NULL ? g_slist_reverse (*the_set) : NULL;
}

/* --------------------------------------------------------------------------------------------- */
