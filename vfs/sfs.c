/*
 * Single File fileSystem
 *
 * Copyright 1998 Pavel Machek, distribute under GPL
 *
 * This defines whole class of filesystems which contain single file
 * inside. It is somehow similar to extfs, except that extfs makes
 * whole virtual trees and we do only single virtual files. */

/*TODO Pridat sfs_fill_names na spravne misto.

Pridat tam nejake add_noncurrent_stamps().

*/

#include <config.h>
#include <errno.h>
#include <sys/types.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "../src/mad.h"
#include "../src/fs.h"

#include <fcntl.h>
#include "../src/util.h"
#include "../src/main.h"

#include "vfs.h"

char *shell = "/bin/bash";

struct cachedfile {
    char *name, *cache;
    uid_t uid;
    struct cachedfile *next;
};

static struct cachedfile *head;

#define MAXFS 32
static int sfs_no = 0;
static char *sfs_prefix[ MAXFS ];
static char *sfs_command[ MAXFS ];
static int sfs_flags[ MAXFS ];
#define F_1 1
#define F_2 2
#define F_NOLOCALCOPY 4
#define F_FULLMATCH 8

static int uptodate( char *name, char *cache )
{
    return 1;
}

static int vfmake( char *name, char *cache )
{
    char *inpath, *op;
    int w;
    char pad [10240];
    char *s, *t = pad;
    int was_percent = 0;

    vfs_split( name, &inpath, &op );
    if ((w = sfs_which( op )) == -1)
        vfs_die( "This cannot happen... Hopefully.\n" );

    if ((sfs_flags[w] & F_1) || (!strcmp( name, "/" ))) ; else return -1;
    /*    if ((sfs_flags[w] & F_2) || (!inpath) || (!*inpath)); else return -1; */
    if (!(sfs_flags[w] & F_NOLOCALCOPY))
        name = mc_getlocalcopy( name );
    else 
        name = strdup( name );
    s = sfs_command[w];
#define COPY_CHAR if (t-pad>10200) return -1; else *t++ = *s;
#define COPY_STRING(a) if ((t-pad)+strlen(a)>10200) return -1; else { strcpy( t, a ); t+= strlen(a); }
    while (*s) {
        if (was_percent) {
	    switch (*s) {
	    case '1': COPY_STRING( name ); break;
	    case '2': COPY_STRING( op + strlen( sfs_prefix[w] ) ); break;
	    case '3': COPY_STRING( cache ); break;
	    case '%': COPY_CHAR; break; 
	    }
	    was_percent = 0;
	} else {
	    if (*s == '%') was_percent = 1;
	              else COPY_CHAR;
	}
	s++;
    }
    free( name );

    if (my_system (EXECUTE_AS_SHELL | EXECUTE_SETUID, shell, pad)) {
	return -1;
    }

    return 0; /* OK */
}

static char *redirect( char *name )
{
    struct cachedfile *cur = head;
    uid_t uid = vfs_uid;
    char *cache, *xname;

    while (cur) {
        if ((!strcmp( name, cur->name )) &&
	    (uid == cur->uid) &&
	    (uptodate( cur->name, cur->cache )))
	    return cur->cache;
	cur = cur->next;
    }
    cache = tempnam( NULL, "sfs" );
    xname = strdup( name );
    if (!vfmake( name, cache )) {
        cur = xmalloc( sizeof(struct cachedfile), "SFS cache" );
	cur->name = xname;
	cur->cache = strdup(cache);
	cur->uid = uid;
	cur->next = head;
	head = cur;
	return cache;
    } else {
        free(xname);
	fprintf( stderr, "vfmake failed\n" );
    }
    return "/I_MUST_NOT_EXIST";
}

#define REDIR path = redirect( path );
    
static void *sfs_open (char *path, int flags, int mode)
{
    int *sfs_info;
    int fd;

    REDIR;
    fd = open (path, flags, mode);
    if (fd == -1)
	return 0;

    sfs_info = (int *) xmalloc (sizeof (int), "SF fs");
    *sfs_info = fd;
    
    return sfs_info;
}

static int sfs_stat (char *path, struct stat *buf)
{
    REDIR;
    return stat (path, buf);
}

static int sfs_lstat (char *path, struct stat *buf)
{
    REDIR;
#ifndef HAVE_STATLSTAT
    return lstat (path,buf);
#else
    return statlstat (path, buf);
#endif
}

static int sfs_chmod (char *path, int mode)
{
    REDIR;
    return chmod (path, mode);
}

static int sfs_chown (char *path, int owner, int group)
{
    REDIR;
    return chown (path, owner, group);
}

static int sfs_utime (char *path, struct utimbuf *times)
{
    REDIR;
    return utime (path, times);
}

static int sfs_readlink (char *path, char *buf, int size)
{
    REDIR;
    return readlink (path, buf, size);
}

#define CUR (*cur)
static vfsid sfs_getid (char *path, struct vfs_stamping **parent)
{	/* FIXME: what should I do? */
    vfs *v;
    vfsid id;
    struct vfs_stamping *par;
    struct cachedfile **cur = &head;

    while (CUR) {
        if ((!strcmp( path, CUR->name )) &&
	    (vfs_uid == CUR->uid))
	    break;
	CUR = CUR->next;
    }
    if (!CUR)
        vfs_die( "sfs_getid of noncached thingie?" );

    *parent = NULL;

    {
        char *path2 = strdup( path );
	v = vfs_split (path2, NULL, NULL);
	id = (*v->getid) (path2, &par);
	free( path2 );
    }

    if (id != (vfsid)-1) {
        *parent = xmalloc (sizeof (struct vfs_stamping), "vfs stamping");
        (*parent)->v = v;
        (*parent)->id = id;
        (*parent)->parent = par;
        (*parent)->next = NULL;
    }
    return (vfsid) cur;
}

static void sfs_free (vfsid id)
{
    struct cachedfile **cur = (struct cachedfile **) id;

    unlink( CUR->cache );
    *cur = CUR->next;
}
#undef CUR

void sfs_fill_names (void (*func)(char *))
{
    struct cachedfile *cur = head;
    char *name;

    while (cur){
	(*func)(cur->name);
	cur = cur->next;
    }
}


static int sfs_nothingisopen (vfsid id)
{
    return 0;
}

static char *sfs_getlocalcopy (char *path)
{
    REDIR;
    return strdup (path);
}

static void sfs_ungetlocalcopy (char *path, char *local, int has_changed)
{
}

#ifdef HAVE_MMAP
static caddr_t sfs_mmap (caddr_t addr, size_t len, int prot, int flags, void *data, off_t offset)
{
    int fd = * (int *)data;

    return mmap (addr, len, prot, flags, fd, offset);
}

static int sfs_munmap (caddr_t addr, size_t len, void *data)
{
    return munmap (addr, len);
}
#endif

extern int local_close (void *data);
extern int local_read (void *data, char *buffer, int count);
extern int local_fstat (void *data, struct stat *buf);
extern int local_errno (void);
extern int local_lseek (void *data, off_t offset, int whence);

vfs sfs_vfs_ops = {
    sfs_open,
    local_close,
    local_read,
    NULL,
    
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    sfs_stat,
    sfs_lstat,
    local_fstat,

    sfs_chmod,
    sfs_chown,
    sfs_utime,

    sfs_readlink,
    NULL,
    NULL,
    NULL,

    NULL,
    NULL,
    local_errno,
    local_lseek,
    NULL,
    
    sfs_getid,
    sfs_nothingisopen,
    sfs_free,
    
    sfs_getlocalcopy,
    sfs_ungetlocalcopy,
    
    NULL,
    NULL,
    
    NULL,
    NULL,
    NULL
#ifdef HAVE_MMAP
    ,sfs_mmap,
    sfs_munmap
#endif
};

void sfs_init (void)
{
    FILE *cfg = fopen( LIBDIR "extfs/sfs.ini", "r" );
    if (!cfg) {
        fprintf( stderr, "Warning: " LIBDIR "extfs/sfs.ini not found\n" );
        return;
    }

    sfs_no = 0;
    while ( sfs_no < MAXFS ) {
        char key[256];
	char *c, *semi = NULL, flags = 0;
	int i;

        if (!fgets( key, 250, cfg ))
	    break;

	if (*key == '#')
	    continue;

	for (i=0; i<strlen(key); i++)
	    if ((key[i]==':') || (key[i]=='/'))
  	        { semi = key+i; if (key[i]=='/') { key[i]=0; flags |= F_FULLMATCH; } break; }

	if (!semi) {
	    fprintf( stderr, "Warning: Invalid line %s in sfs.ini.\n", key );
	    continue;
	}

	c = semi + 1;
	while ((*c != ' ') && (*c != 9)) {
	    switch (*c) {
	    case '1': flags |= F_1; break;
	    case '2': flags |= F_2; break;
	    case 'R': flags |= F_NOLOCALCOPY; break;
	    default:
	      fprintf( stderr, "Warning: Invalid flag %c in sfs.ini line %s.\n", c, key );
	    }	    
	    c++;
	}
	c++;
	*(semi+1) = 0;
	if ((semi = strchr( c, '\n')))
	    *semi = 0;

	sfs_prefix [sfs_no] = strdup (key);
	sfs_command [sfs_no] = strdup (c);
	sfs_flags [sfs_no] = flags;
	sfs_no++;
    }
    fclose(cfg);
}

int sfs_which (char *path)
{
    int i;

    for (i = 0; i < sfs_no; i++)
        if (sfs_flags [i] & F_FULLMATCH) {
	    if (!strcmp (path, sfs_prefix [i]))
	        return i;
	} else
	    if (!strncmp (path, sfs_prefix [i], strlen( sfs_prefix[i]) ))
	        return i;

    
    return -1;
}
