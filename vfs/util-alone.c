/*
 * Author: 1998 Pavel Machek <pavel@ucw.cz>
 *
 * This is for making midnight commander's vfs stuff compile stand-alone
 *
 * Namespace pollution: horrible
 */

#include <config.h>
#include <stdio.h>
#if defined(__os2__)            /* OS/2 need io.h! .ado */
#    include <io.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <signal.h>		/* my_system */
#include <limits.h>		/* INT_MAX */
#ifndef SCO_FLAVOR
#	include <sys/time.h>	/* alex: sys/select.h defines struct timeval */
#endif /* SCO_FLAVOR */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>		/* my_system */
#ifdef SCO_FLAVOR
#	include <sys/timeb.h>	/* alex: for struct timeb, used in time.h */
#endif /* SCO_FLAVOR */
#include <time.h>
#ifndef OS2_NT
#   include <pwd.h>
#   include <grp.h>
#endif
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#ifdef __linux__
#    if defined(__GLIBC__) && (__GLIBC__ < 2)
#        include <linux/termios.h>	/* This is needed for TIOCLINUX */
#    else
#        include <termios.h>
#    endif
#  include <sys/ioctl.h>
#endif

#include "../src/util.h"
#include "vfs.h"
#include "callback.h"

#ifndef VFS_STANDALONE
#error This has only sense when compiling standalone version
#endif

int source_route = 0;
int cd_symlinks = 0;

/*
 * We do not want/need many of midnight's functions, stub routines.
 */

void
enable_interrupt_key (void)
{
}

void
disable_interrupt_key (void)
{
}

int  got_interrupt (void)
{
	return 0;
}

void
rotate_dash (void)
{
}

char *
load_anon_passwd (void)
{
	return NULL;
}

static char (*callbacks[NUM_CALLBACKS])(char *msg) = { NULL, NULL, NULL, };

void
vfs_set_callback (int num, void *func)
{
    if (num >= NUM_CALLBACKS)
        vfs_die ("Attempt to set invalid callback.\n");
    callbacks [num] = func;
}

static void
info_puts( char *s )
{
    if (!callbacks [CALL_INFO])
        fprintf (stderr, "%s\n", s);
    else
        callbacks [CALL_INFO](s);
}

static void
box_puts( char *s )
{
    if (!callbacks [CALL_BOX])
        fprintf (stderr, "%s\n", s);
    else
        callbacks [CALL_BOX](s);
}

char *
vfs_get_password (char *msg)
{
    if (!callbacks [CALL_PASSWD])
        return NULL;
    else
        callbacks [CALL_PASSWD](msg);
}

void
print_vfs_message (char *msg, ...)
{
    char *str;
    va_list args;

    va_start  (args,msg);
    str = g_strdup_vprintf (msg, args);
    va_end    (args);

    info_puts (str);
    g_free (str);
}

void
wipe_password (char *passwd)
{
    char *p = passwd;

    if (p == NULL)
	    return;
    
    for (;*p; p++)
        *p = 0;
    g_free (passwd);
}

int
exist_file (char *name)
{
    return access (name, R_OK) == 0;
}

void
message_1s (int i, char *c1, char *c2)
{
    char buf [4096];

    snprintf (buf, sizeof (buf), "%s %s", c1, c2);
    box_puts (buf);
}

void
message_2s (int i, char *c1, char *c2, char *c3)
{
    char buf [4096];
    
    snprintf (buf, sizeof (buf), "%s %s %s", c1, c2, c3 );
    box_puts (buf );
}

void
message_3s( int i, char *c1, char *c2, char *c3, const char *c4 )
{
    char buf [4096];
    
    snprintf (buf, sizeof (buf), "%s %s %s %s", c1, c2, c3, c4);
    box_puts (buf);
}

void vfs_init( void );
void ftpfs_init_passwd( void );

char *mc_home = LIBDIR;

void
mc_vfs_init( void )
{
    vfs_init();
    ftpfs_init_passwd();
}

void
mc_vfs_done( void )
{
    vfs_shut();
}
