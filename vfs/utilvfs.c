/* Utilities for VFS modules.

   Copyright (C) 1988, 1992, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.
   Copyright (C) 1995, 1996 Miguel de Icaza
   
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

#include <config.h>

#include <ctype.h>

#include <mhl/types.h>
#include <mhl/memory.h>
#include <mhl/string.h>

#include "../src/global.h"
#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */
#include "utilvfs.h"
#include "vfs.h"
#include "../src/unixcompat.h"
#include "../src/history.h"

/* Extract the hostname and username from the path */
/* path is in the form: [user@]hostname:port/remote-dir, e.g.:
 *
 * ftp://sunsite.unc.edu/pub/linux
 * ftp://miguel@sphinx.nuclecu.unam.mx/c/nc
 * ftp://tsx-11.mit.edu:8192/
 * ftp://joe@foo.edu:11321/private
 * ftp://joe:password@foo.se
 *
 * Returns g_malloc()ed host, user and pass they are present.
 * If the user is empty, e.g. ftp://@roxanne/private, and URL_ALLOW_ANON
 * is not set, then the current login name is supplied.
 *
 * Return value is a g_malloc()ed string with the pathname relative to the
 * host.
 */

#ifdef USE_NETCODE
char *
vfs_split_url (const char *path, char **host, char **user, int *port,
	       char **pass, int default_port, int flags)
{
    struct passwd *passwd_info;
    char *dir, *colon, *inner_colon, *at, *rest;
    char *retval;
    char * const pcopy = g_strdup (path);
    const char *pend = pcopy + strlen (pcopy);

    if (pass)
	*pass = NULL;
    *port = default_port;
    *user = NULL;
    retval = NULL;

    dir = pcopy;
    if (!(flags & URL_NOSLASH)) {
	/* locate path component */
	while (*dir != PATH_SEP && *dir)
	    dir++;
	if (*dir) {
	    retval = g_strdup (dir);
	    *dir = 0;
	} else
	    retval = g_strdup (PATH_SEP_STR);
    }

    /* search for any possible user */
    at = strrchr (pcopy, '@');

    /* We have a username */
    if (at) {
	*at = 0;
	inner_colon = strchr (pcopy, ':');
	if (inner_colon) {
	    *inner_colon = 0;
	    inner_colon++;
	    if (pass)
		*pass = g_strdup (inner_colon);
	}
	if (*pcopy != 0)
	    *user = g_strdup (pcopy);

	if (pend == at + 1)
	    rest = at;
	else
	    rest = at + 1;
    } else
	rest = pcopy;

    if (!*user && !(flags & URL_ALLOW_ANON)) {
	passwd_info = getpwuid (geteuid ());
	if (passwd_info && passwd_info->pw_name)
	    *user = g_strdup (passwd_info->pw_name);
	else {
	    /* This is very unlikely to happen */
	    *user = g_strdup ("anonymous");
	}
	endpwent ();
    }

    /* Check if the host comes with a port spec, if so, chop it */
    colon = strchr (rest, ':');
    if (colon) {
	*colon = 0;
	if (sscanf (colon + 1, "%d", port) == 1) {
	    if (*port <= 0 || *port >= 65536)
		*port = default_port;
	} else {
	    while (*(++colon)) {
		switch (*colon) {
		case 'C':
		    *port = 1;
		    break;
		case 'r':
		    *port = 2;
		    break;
		}
	    }
	}
    }
    if (host)
	*host = g_strdup (rest);

    g_free (pcopy);
    return retval;
}
#endif				/* USE_NETCODE */

/*
 * Look up a user or group name from a uid/gid, maintaining a cache.
 * FIXME, for now it's a one-entry cache.
 * FIXME2, the "-993" is to reduce the chance of a hit on the first lookup.
 * This file should be modified for non-unix systems to do something
 * reasonable.
 */

#ifndef TUNMLEN
#define TUNMLEN 256
#endif
#ifndef TGNMLEN
#define TGNMLEN 256
#endif

#define myuid	( my_uid < 0? (my_uid = getuid()): my_uid )
#define	mygid	( my_gid < 0? (my_gid = getgid()): my_gid )

int
vfs_finduid (const char *uname)
{
    static int saveuid = -993;
    static char saveuname[TUNMLEN];
    static int my_uid = -993;

    struct passwd *pw;

    if (uname[0] != saveuname[0]	/* Quick test w/o proc call */
	||0 != strncmp (uname, saveuname, TUNMLEN)) {
	g_strlcpy (saveuname, uname, TUNMLEN);
	pw = getpwnam (uname);
	if (pw) {
	    saveuid = pw->pw_uid;
	} else {
	    saveuid = myuid;
	}
    }
    return saveuid;
}

int
vfs_findgid (const char *gname)
{
    static int savegid = -993;
    static char savegname[TGNMLEN];
    static int my_gid = -993;

    struct group *gr;

    if (gname[0] != savegname[0]	/* Quick test w/o proc call */
	||0 != strncmp (gname, savegname, TUNMLEN)) {
	g_strlcpy (savegname, gname, TUNMLEN);
	gr = getgrnam (gname);
	if (gr) {
	    savegid = gr->gr_gid;
	} else {
	    savegid = mygid;
	}
    }
    return savegid;
}

/*
 * Create a temporary file with a name resembling the original.
 * This is needed e.g. for local copies requested by extfs.
 * Some extfs scripts may look at the extension.
 * We also protect stupid scripts agains dangerous names.
 */
int
vfs_mkstemps (char **pname, const char *prefix, const char *basename)
{
    const char *p;
    char *suffix, *q;
    int shift;
    int fd;

    /* Strip directories */
    p = strrchr (basename, PATH_SEP);
    if (!p)
	p = basename;
    else
	p++;

    /* Protection against very long names */
    shift = strlen (p) - (MC_MAXPATHLEN - 16);
    if (shift > 0)
	p += shift;

    suffix = g_malloc (MC_MAXPATHLEN);

    /* Protection against unusual characters */
    q = suffix;
    while (*p && (*p != '#')) {
	if (strchr (".-_@", *p) || isalnum ((unsigned char) *p))
	    *q++ = *p;
	p++;
    }
    *q = 0;

    fd = mc_mkstemps (pname, prefix, suffix);
    g_free (suffix);
    return fd;
}

/* Parsing code is used by ftpfs, fish and extfs */
#define MAXCOLS		30

static char *columns[MAXCOLS];	/* Points to the string in column n */
static int column_ptr[MAXCOLS];	/* Index from 0 to the starting positions of the columns */

int
vfs_split_text (char *p)
{
    char *original = p;
    int numcols;

    memset (columns, 0, sizeof (columns));

    for (numcols = 0; *p && numcols < MAXCOLS; numcols++) {
	while (*p == ' ' || *p == '\r' || *p == '\n') {
	    *p = 0;
	    p++;
	}
	columns[numcols] = p;
	column_ptr[numcols] = p - original;
	while (*p && *p != ' ' && *p != '\r' && *p != '\n')
	    p++;
    }
    return numcols;
}

static int
is_num (int idx)
{
    char *column = columns[idx];

    if (!column || column[0] < '0' || column[0] > '9')
	return 0;

    return 1;
}

/* Return 1 for MM-DD-YY and MM-DD-YYYY */
static int
is_dos_date (const char *str)
{
    int len;

    if (!str)
	return 0;

    len = strlen (str);
    if (len != 8 && len != 10)
	return 0;

    if (str[2] != str[5])
	return 0;

    if (!strchr ("\\-/", (int) str[2]))
	return 0;

    return 1;
}

static int
is_week (const char *str, struct tm *tim)
{
    static const char *week = "SunMonTueWedThuFriSat";
    const char *pos;

    if (!str)
	return 0;

    if ((pos = strstr (week, str)) != NULL) {
	if (tim != NULL)
	    tim->tm_wday = (pos - week) / 3;
	return 1;
    }
    return 0;
}

static int
is_month (const char *str, struct tm *tim)
{
    static const char *month = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *pos;

    if (!str)
	return 0;

    if ((pos = strstr (month, str)) != NULL) {
	if (tim != NULL)
	    tim->tm_mon = (pos - month) / 3;
	return 1;
    }
    return 0;
}

/*
 * Check for possible locale's abbreviated month name (Jan..Dec).
 * Any 3 bytes long string without digit, control and punctuation characters.
 * isalpha() is locale specific, so it cannot be used if current
 * locale is "C" and ftp server use Cyrillic.
 * NB: It is assumed there are no whitespaces in month.
 */
static int
is_localized_month (const char *month)
{
    int i = 0;

    if (!month)
	return 0;

    while ((i < 3) && *month && !isdigit ((unsigned char) *month)
	   && !iscntrl ((unsigned char) *month)
	   && !ispunct ((unsigned char) *month)) {
	i++;
	month++;
    }
    return ((i == 3) && (*month == 0));
}

static int
is_time (const char *str, struct tm *tim)
{
    const char *p, *p2;

    if (!str)
	return 0;

    if ((p = strchr (str, ':')) && (p2 = strrchr (str, ':'))) {
	if (p != p2) {
	    if (sscanf
		(str, "%2d:%2d:%2d", &tim->tm_hour, &tim->tm_min,
		 &tim->tm_sec) != 3)
		return 0;
	} else {
	    if (sscanf (str, "%2d:%2d", &tim->tm_hour, &tim->tm_min) != 2)
		return 0;
	}
    } else
	return 0;

    return 1;
}

static int
is_year (char *str, struct tm *tim)
{
    long year;

    if (!str)
	return 0;

    if (strchr (str, ':'))
	return 0;

    if (strlen (str) != 4)
	return 0;

    if (sscanf (str, "%ld", &year) != 1)
	return 0;

    if (year < 1900 || year > 3000)
	return 0;

    tim->tm_year = (int) (year - 1900);

    return 1;
}

gboolean
vfs_parse_filetype (const char *s, size_t *ret_skipped, mode_t *ret_type)
{
    mode_t type;

    switch (*s) {
	case 'd': type = S_IFDIR; break;
	case 'b': type = S_IFBLK; break;
	case 'c': type = S_IFCHR; break;
	case 'l': type = S_IFLNK; break;
#ifdef S_IFSOCK
	case 's': type = S_IFSOCK; break;
#else
	case 's': type = S_IFIFO; break;
#endif
#ifdef S_IFDOOR			/* Solaris door */
	case 'D': type = S_IFDOOR; break;
#else
	case 'D': type = S_IFIFO; break;
#endif
	case 'p': type = S_IFIFO; break;
#ifdef S_IFNAM			/* Special named files */
	case 'n': type = S_IFNAM; break;
#else
	case 'n': type = S_IFREG; break;
#endif
	case 'm':		/* Don't know what these are :-) */
	case '-':
	case '?': type = S_IFREG; break;
	default: return FALSE;
    }

    *ret_type = type;
    *ret_skipped = 1;
    return TRUE;
}

gboolean
vfs_parse_fileperms (const char *s, size_t *ret_skipped, mode_t *ret_perms)
{
    const char *p;
    mode_t perms;

    p = s;
    perms = 0;

    switch (*p++) {
	case '-': break;
	case 'r': perms |= S_IRUSR; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'w': perms |= S_IWUSR; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'S': perms |= S_ISUID; break;
	case 's': perms |= S_IXUSR | S_ISUID; break;
	case 'x': perms |= S_IXUSR; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'r': perms |= S_IRGRP; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'w': perms |= S_IWGRP; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'S': perms |= S_ISGID; break;
	case 'l': perms |= S_ISGID; break; /* found on Solaris */
	case 's': perms |= S_IXGRP | S_ISGID; break;
	case 'x': perms |= S_IXGRP; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'r': perms |= S_IROTH; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'w': perms |= S_IWOTH; break;
	default: return FALSE;
    }
    switch (*p++) {
	case '-': break;
	case 'T': perms |= S_ISVTX; break;
	case 't': perms |= S_IXOTH | S_ISVTX; break;
	case 'x': perms |= S_IXOTH; break;
	default: return FALSE;
    }
    if (*p == '+') {		/* ACLs on Solaris, HP-UX and others */
	p++;
    }

    *ret_skipped = p - s;
    *ret_perms = perms;
    return TRUE;
}

gboolean
vfs_parse_filemode (const char *s, size_t *ret_skipped,
		    mode_t *ret_mode)
{
    const char *p;
    mode_t type, perms;
    size_t skipped;

    p = s;

    if (!vfs_parse_filetype (p, &skipped, &type))
	return FALSE;
    p += skipped;

    if (!vfs_parse_fileperms (p, &skipped, &perms))
	return FALSE;
    p += skipped;

    *ret_skipped = p - s;
    *ret_mode = type | perms;
    return TRUE;
}

gboolean
vfs_parse_raw_filemode (const char *s, size_t *ret_skipped,
		    mode_t *ret_mode)
{
    const char *p;
    mode_t remote_type = 0, local_type, perms = 0;

    p = s;

    // isoctal
    while(*p >= '0' && *p <= '7')
    {
	perms *= 010;
	perms += (*p - '0');
	++p;
    }

    if (*p++ != ' ')
	return FALSE;

    while(*p >= '0' && *p <= '7')
    {
	remote_type *= 010;
	remote_type += (*p - '0');
	++p;
    }

    if (*p++ != ' ')
	return FALSE;

    /* generated with:
        $ perl -e 'use Fcntl ":mode";
                   my @modes = (S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFREG);
                   foreach $t (@modes) { printf ("%o\n", $t); };'
        TODO: S_IFDOOR, S_IFIFO, S_IFSOCK (if supported by os)
              (see vfs_parse_filetype)
     */

    switch (remote_type)
    {
	case 020000:
	    local_type = S_IFCHR; break;
	case 040000:
	    local_type = S_IFDIR; break;
	case 060000:
	    local_type = S_IFBLK; break;
	case 0120000:
	    local_type = S_IFLNK; break;
	case 0100000:
	default:// don't know what is it
	    local_type = S_IFREG; break;
    }

    *ret_skipped = p - s;
    *ret_mode = local_type | perms;
    return TRUE;
}

/* This function parses from idx in the columns[] array */
int
vfs_parse_filedate (int idx, time_t *t)
{
    char *p;
    struct tm tim;
    int d[3];
    int got_year = 0;
    int l10n = 0;		/* Locale's abbreviated month name */
    time_t current_time;
    struct tm *local_time;

    /* Let's setup default time values */
    current_time = time (NULL);
    local_time = localtime (&current_time);
    tim.tm_mday = local_time->tm_mday;
    tim.tm_mon = local_time->tm_mon;
    tim.tm_year = local_time->tm_year;

    tim.tm_hour = 0;
    tim.tm_min = 0;
    tim.tm_sec = 0;
    tim.tm_isdst = -1;		/* Let mktime() try to guess correct dst offset */

    p = columns[idx++];

    /* We eat weekday name in case of extfs */
    if (is_week (p, &tim))
	p = columns[idx++];

    /* Month name */
    if (is_month (p, &tim)) {
	/* And we expect, it followed by day number */
	if (is_num (idx))
	    tim.tm_mday = (int) atol (columns[idx++]);
	else
	    return 0;		/* No day */

    } else {
	/* We expect:
	   3 fields max or we'll see oddities with certain file names.
	   So both year and time is not allowed.
	   Mon DD hh:mm[:ss]
	   Mon DD YYYY
	   But in case of extfs we allow these date formats:
	   MM-DD-YY hh:mm[:ss]
	   where Mon is Jan-Dec, DD, MM, YY two digit day, month, year,
	   YYYY four digit year, hh, mm, ss two digit hour, minute or second. */

	/* Special case with MM-DD-YY or MM-DD-YYYY */
	if (is_dos_date (p)) {
	    p[2] = p[5] = '-';

	    if (sscanf (p, "%2d-%2d-%d", &d[0], &d[1], &d[2]) == 3) {
		/* Months are zero based */
		if (d[0] > 0)
		    d[0]--;

		if (d[2] > 1900) {
		    d[2] -= 1900;
		} else {
		    /* Y2K madness */
		    if (d[2] < 70)
			d[2] += 100;
		}

		tim.tm_mon = d[0];
		tim.tm_mday = d[1];
		tim.tm_year = d[2];
		got_year = 1;
	    } else
		return 0;	/* sscanf failed */
	} else {
	    /* Locale's abbreviated month name followed by day number */
	    if (is_localized_month (p) && (is_num (idx++)))
		l10n = 1;
	    else
		return 0;	/* unsupported format */
	}
    }

    /* Here we expect to find time or year */
    if (is_num (idx) && (is_time (columns[idx], &tim)
	    || (got_year = is_year (columns[idx], &tim))))
	    idx++;
    else
	return 0;		/* Neither time nor date */

    /*
     * If the date is less than 6 months in the past, it is shown without year
     * other dates in the past or future are shown with year but without time
     * This does not check for years before 1900 ... I don't know, how
     * to represent them at all
     */
    if (!got_year && local_time->tm_mon < 6
	&& local_time->tm_mon < tim.tm_mon
	&& tim.tm_mon - local_time->tm_mon >= 6)

	tim.tm_year--;

    if (l10n || (*t = mktime (&tim)) < 0)
	*t = 0;
    return idx;
}

int
vfs_parse_ls_lga (const char *p, struct stat *s, char **filename,
		  char **linkname)
{
    int idx, idx2, num_cols;
    int i;
    char *p_copy = NULL;
    char *t = NULL;
    const char *line = p;
    size_t skipped;

    if (strncmp (p, "total", 5) == 0)
	return 0;

    if (!vfs_parse_filetype (p, &skipped, &s->st_mode))
	goto error;
    p += skipped;

    if (*p == ' ')		/* Notwell 4 */
	p++;
    if (*p == '[') {
	if (strlen (p) <= 8 || p[8] != ']')
	    goto error;
	/* Should parse here the Notwell permissions :) */
	if (S_ISDIR (s->st_mode))
	    s->st_mode |=
		(S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IXUSR | S_IXGRP
		 | S_IXOTH);
	else
	    s->st_mode |= (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
	p += 9;
    } else {
	size_t skipped;
	mode_t perms;

	if (!vfs_parse_fileperms (p, &skipped, &perms))
	    goto error;
	p += skipped;
	s->st_mode |= perms;
    }

    p_copy = g_strdup (p);
    num_cols = vfs_split_text (p_copy);

    s->st_nlink = atol (columns[0]);
    if (s->st_nlink <= 0)
	goto error;

    if (!is_num (1))
	s->st_uid = vfs_finduid (columns[1]);
    else
	s->st_uid = (uid_t) atol (columns[1]);

    /* Mhm, the ls -lg did not produce a group field */
    for (idx = 3; idx <= 5; idx++)
	if (is_month (columns[idx], NULL) || is_week (columns[idx], NULL)
	    || is_dos_date (columns[idx])
	    || is_localized_month (columns[idx]))
	    break;

    if (idx == 6
	|| (idx == 5 && !S_ISCHR (s->st_mode) && !S_ISBLK (s->st_mode)))
	goto error;

    /* We don't have gid */
    if (idx == 3
	|| (idx == 4 && (S_ISCHR (s->st_mode) || S_ISBLK (s->st_mode))))
	idx2 = 2;
    else {
	/* We have gid field */
	if (is_num (2))
	    s->st_gid = (gid_t) atol (columns[2]);
	else
	    s->st_gid = vfs_findgid (columns[2]);
	idx2 = 3;
    }

    /* This is device */
    if (S_ISCHR (s->st_mode) || S_ISBLK (s->st_mode)) {
	int maj, min;

	/* Corner case: there is no whitespace(s) between maj & min */
	if (!is_num (idx2) && idx2 == 2) {
	    if (!is_num (++idx2) || sscanf (columns[idx2], " %d,%d", &min, &min) != 2)
		goto error;
	} else {
	    if (!is_num (idx2) || sscanf (columns[idx2], " %d,", &maj) != 1)
		goto error;

	    if (!is_num (++idx2) || sscanf (columns[idx2], " %d", &min) != 1)
		goto error;
	}
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	s->st_rdev = makedev (maj, min);
#endif
	s->st_size = 0;

    } else {
	/* Common file size */
	if (!is_num (idx2))
	    goto error;

#ifdef HAVE_ATOLL
	s->st_size = (off_t) atoll (columns[idx2]);
#else
	s->st_size = (off_t) atof (columns[idx2]);
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
	s->st_rdev = 0;
#endif
    }

    idx = vfs_parse_filedate (idx, &s->st_mtime);
    if (!idx)
	goto error;
    /* Use resulting time value */
    s->st_atime = s->st_ctime = s->st_mtime;
    /* s->st_dev and s->st_ino must be initialized by vfs_s_new_inode () */
#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    s->st_blksize = 512;
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    s->st_blocks = (s->st_size + 511) / 512;
#endif

    for (i = idx + 1, idx2 = 0; i < num_cols; i++)
	if (strcmp (columns[i], "->") == 0) {
	    idx2 = i;
	    break;
	}

    if (((S_ISLNK (s->st_mode) || (num_cols == idx + 3 && s->st_nlink > 1)))	/* Maybe a hardlink? (in extfs) */
	&&idx2) {

	if (filename) {
	    *filename =
		g_strndup (p + column_ptr[idx],
			   column_ptr[idx2] - column_ptr[idx] - 1);
	}
	if (linkname) {
	    t = g_strdup (p + column_ptr[idx2 + 1]);
	    *linkname = t;
	}
    } else {
	/* Extract the filename from the string copy, not from the columns
	 * this way we have a chance of entering hidden directories like ". ."
	 */
	if (filename) {
	    /*
	     * filename = g_strdup (columns [idx++]);
	     */

	    t = g_strdup (p + column_ptr[idx]);
	    *filename = t;
	}
	if (linkname)
	    *linkname = NULL;
    }

    if (t) {
	int p = strlen (t);
	if ((--p > 0) && (t[p] == '\r' || t[p] == '\n'))
	    t[p] = 0;
	if ((--p > 0) && (t[p] == '\r' || t[p] == '\n'))
	    t[p] = 0;
    }

    g_free (p_copy);
    return 1;

  error:
    {
	static int errorcount = 0;

	if (++errorcount < 5) {
	    message (D_ERROR, _("Cannot parse:"), "%s",
		     (p_copy && *p_copy) ? p_copy : line);
	} else if (errorcount == 5)
	    message (D_ERROR, MSG_ERROR,
		     _("More parsing errors will be ignored."));
    }

    g_free (p_copy);
    return 0;
}

void
vfs_die (const char *m)
{
    message (D_ERROR, _("Internal error:"), "%s", m);
    exit (1);
}

char *
vfs_get_password (const char *msg)
{
    return input_dialog (msg, _("Password:"), MC_HISTORY_VFS_PASSWORD, INPUT_PASSWORD);
}
