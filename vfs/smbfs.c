/* Virtual File System: Midnight Commander file system.
   
   Copyright (C) 1995, 1996, 1997 The Free Software Foundation

   Written by Wayne Roberts
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Namespace: exports smbfs_vfs_ops, tcp_invalidate_socket */

#include "samba/include/includes.h"

#include "utilvfs.h"

#include "vfs.h"
#include "smbfs.h"
#include "tcputil.h"
#include "../src/dialog.h"

#define SMBFS_MAX_CONNECTIONS 16

static int my_errno;

/* stuff that is same with each connection */
extern int DEBUGLEVEL;
static pstring myhostname;
static pstring workgroup;
mode_t myumask = 0755;
extern pstring global_myname;
static int smbfs_open_connections = 0;
char *IPC = "IPC$";
gboolean got_user = FALSE;
gboolean got_pass = FALSE;
pstring password;
pstring username;

static struct _smbfs_connection {
	struct cli_state *cli;
	struct in_addr dest_ip;
	BOOL have_ip;
	pstring password;
	pstring service;	/* share name */
    char *host;			/* server name */
	char *user;
	char *home;
	int port;
	int name_type;
} smbfs_connections [SMBFS_MAX_CONNECTIONS];
/* unique to each connection */

typedef struct _smbfs_connection smbfs_connection;
smbfs_connection *current_bucket;

typedef struct {
	struct cli_state *cli;
	int fnum;
	off_t nread;
	uint16 attr;
} smbfs_handle;

void smbfs_set_debug(int arg) {
	DEBUGLEVEL = arg;
}

/********************** The callbacks ******************************/
int smbfs_init(vfs *me) {
	char *servicesf = "/etc/smb.conf";

/*	DEBUGLEVEL = 4;	*/

	setup_logging("mc", True);
    TimeInit();
    charset_initialise();

	DEBUG(4,("smbfs_init(%s)\n", me->name));

	if(!get_myname(myhostname,NULL))
        DEBUG(0,("Failed to get my hostname.\n"));

	if (!lp_load(servicesf,True,False,False))
		DEBUG(0, ("Can't load %s - run testparm to debug it\n", servicesf));

    codepage_initialise(lp_client_code_page());
    pstrcpy(workgroup,lp_workgroup());
	DEBUG(4,("workgroup: %s\n", workgroup));

    load_interfaces();
    myumask = umask(0);
    umask(myumask);

    if (getenv("USER")) {
		char *p;
        pstrcpy(username, getenv("USER"));
		got_user = TRUE;
		DEBUG(4, ("smbfs_init(): $USER:%s\n", username));
        if ((p = strchr(username, '%'))) {
            *p = 0;
            pstrcpy(password, p+1);
            got_pass = TRUE;
            memset(strchr(getenv("USER"), '%')+1, 'X', strlen(password));
			DEBUG(4, ("smbfs_init(): $USER%%pass: %s%%%s\n",
				username, password));
        }
        strupper(username);
    }
    if (getenv("PASSWD")) {
        pstrcpy(password, getenv("PASSWD"));
        got_pass = TRUE;
    }
	return 1;
}

static void smbfs_fill_names (vfs *me, void (*func)(char *)) {
	DEBUG(4, ("smbfs_fill_names()\n"));
}

#define CNV_LANG(s) dos_to_unix(s,False)
/* does same as do_get() in client.c */
/* called from vfs.c:1080, count = buffer size */
static int smbfs_read (void *data, char *buffer, int count)
{
    smbfs_handle *info = (smbfs_handle *) data;
	int n;

	DEBUG(4, ("smbfs_read(fnum:%d, nread:%d, count:%d)\n",
		info->fnum, (int)info->nread, count));
	n = cli_read(info->cli, info->fnum, buffer, info->nread, count);
	if (n > 0)
		info->nread += n;
	return n;
}

static int smbfs_write (void *data, char *buf, int nbyte)
{
    smbfs_handle *info = (smbfs_handle *) data;
	int n;

	DEBUG(4, ("smbfs_write(fnum:%d, nread:%d, nbyte:%d)\n",
		info->fnum, (int)info->nread, nbyte));
	n = cli_write(info->cli, info->fnum, 0, buf, info->nread, nbyte);
	if (n > 0)
		info->nread += n;
    return n;
}

static int smbfs_close (void *data)
{
    smbfs_handle *info = (smbfs_handle *) data;
	DEBUG(4,("smbfs_close(fnum:%d)\n", info->fnum));
/* if imlementing archive_level:	add rname to smbfs_handle
    if (archive_level >= 2 && (inf->attr & aARCH)) {
        cli_setatr(info->cli, rname, info->attr & ~(uint16)aARCH, 0);
    }	*/
	return cli_close(info->cli, info->fnum);
}

static int smbfs_errno (vfs *me)
{
	DEBUG(4,("smbfs_errno: %s\n", g_strerror(my_errno)));
    return my_errno;
}

typedef struct dir_entry {
    char *text;
    struct dir_entry *next;
    struct stat my_stat;
    int    merrno;
} dir_entry;

typedef struct {
	char *dirname;
	pstring path;
    smbfs_connection *conn;
    dir_entry *entries;
    dir_entry *current;
} opendir_info;

opendir_info *current_info;	/* FIXME: must be moved into smbfs_connection? */
gboolean first_direntry;
void browsing_helper(const char *name, uint32 type, const char *comment) {
	fstring typestr;
	dir_entry *new_entry;
	new_entry = g_new (dir_entry, 1);
    new_entry->text = g_new0 (char, strlen(name)+1);
	pstrcpy(new_entry->text, name);

    new_entry->next = 0;

	if (first_direntry) {
		current_info->entries = new_entry;
		current_info->current = new_entry;
		first_direntry = FALSE;
	} else {
		current_info->current->next = new_entry;
		current_info->current = new_entry;
	}

	bzero(&new_entry->my_stat, sizeof(struct stat));
	new_entry->merrno = 0;
	*typestr=0;
	switch (type) {
		case STYPE_DISKTREE:
			fstrcpy(typestr,"Disk");
			/*	show this as file	*/
			new_entry->my_stat.st_mode = S_IFDIR|S_IRUSR|S_IRGRP|S_IROTH;
			break;
		case STYPE_PRINTQ:
			fstrcpy(typestr,"Printer"); break;
		case STYPE_DEVICE:
			fstrcpy(typestr,"Device"); break;
		case STYPE_IPC:
			fstrcpy(typestr,"IPC"); break;
	}
	DEBUG(4,("\t%-15.15s%-10.10s%s\n", name, typestr,comment)); 
}

static void loaddir_helper(file_info *finfo, const char *mask) {
	time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */

	dir_entry *new_entry;
	new_entry = g_new (dir_entry, 1);
    new_entry->text = g_new0 (char, strlen(finfo->name)+1);
	pstrcpy(new_entry->text, finfo->name);

    new_entry->next = 0;

	if (first_direntry) {
		current_info->entries = new_entry;
		current_info->current = new_entry;
		first_direntry = FALSE;
	} else {
		current_info->current->next = new_entry;
		current_info->current = new_entry;
	}

	new_entry->my_stat.st_size = finfo->size;
	new_entry->my_stat.st_mtime = finfo->mtime;
	new_entry->my_stat.st_atime = finfo->atime;
	new_entry->my_stat.st_ctime = finfo->ctime;
	new_entry->my_stat.st_uid = finfo->uid;
	new_entry->my_stat.st_gid = finfo->gid;


//	new_entry->my_stat.st_mode = finfo->mode;
	new_entry->my_stat.st_mode =	S_IRUSR|S_IRGRP|S_IROTH |
									S_IWUSR|S_IWGRP|S_IWOTH;
								
/*  if (finfo->mode & aVOLID);	 nothing similar in real world */
	if (finfo->mode & aDIR)
		new_entry->my_stat.st_mode |= S_IFDIR;
	else
		new_entry->my_stat.st_mode |= S_IFREG;	/* if not dir, regular file? */
/*  if (finfo->mode & aARCH);	DOS archive	*/
/*  if (finfo->mode & aHIDDEN);	like a dot file? */
/*  if (finfo->mode & aSYSTEM); like a kernel? */
  if (finfo->mode & aRONLY)
		new_entry->my_stat.st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
 
	DEBUG(4, ("  %-30s%7.7s%8.0f  %s",
		CNV_LANG(finfo->name),
		attrib_string(finfo->mode),
		(double)finfo->size,
		asctime(LocalTime(&t)))); 
}

/* takes "/foo/bar/file" and gives "\\foo\\bar\\file" */
int convert_path(char **remote_file, gboolean trailing_asterik) {
	char *p1, *p2;
	char *my_remote;
	char *rname = g_new(char, strlen(*remote_file)*2);
	my_remote = g_new(char, strlen(*remote_file)+1);
	my_remote = g_strdup(*remote_file);

    if (strncmp (my_remote, "/#smb:", 6) == 0) {	/* if passed directly */
		my_remote++;	/* cant have leading '/' */
		p1 = strchr(my_remote, '/');
		if (p1)
			my_remote = p1+1;	/* advance to end of server name */
	}

    if (*my_remote == '/')
        my_remote++;  /* strip off leading '/' */
    p1 = strchr(my_remote, '/');
    if (p1)
        my_remote = p1;   /* strip off share/service name */
    /* create remote filename as understood by smb clientgen */
    rname[0] = 0;
    while ((p1 = strchr(my_remote, '/'))) {
        pstrcat(rname, "\\");
        p1++;
        p2 = strchr(p1, '/');
        if (p2) {
            strncat(rname, p1, p2-p1);
            my_remote = p2;
        } else {    /* last time */
            my_remote = p1;
            pstrcat(rname, my_remote);
        }
    }
	if (trailing_asterik)
		pstrcat(rname, "\\*");	/* for directory listings */
	*remote_file = g_strdup(rname);
	return 0;
}

static int smbfs_loaddir (opendir_info *smbfs_info) {
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	int servlen = strlen(smbfs_info->conn->service);
	char *p;
	char *my_dirname = g_new(char, sizeof(smbfs_info->dirname)+1);
	my_dirname = g_strdup(smbfs_info->dirname);
	p = my_dirname;

	DEBUG(4,("smbfs_loaddir: dirname:%s\n", my_dirname));
	first_direntry = TRUE;
	current_info = smbfs_info;

	if (strcmp(my_dirname, "/") == 0) {	/* browse for shares */
		DEBUG(4,("smbfs_loaddir: browsing IPC$\n"));
		if (!cli_RNetShareEnum(smbfs_info->conn->cli, browsing_helper))
			return 0;
		goto done;
	}
	/* do regular directory listing */
	if(strncmp(smbfs_info->conn->service, my_dirname+1, servlen) == 0) {
		/* strip share name from dir */
		my_dirname += servlen;
		*my_dirname = '/';
	}
	convert_path(&my_dirname, TRUE);

	DEBUG(4,("smbfs_loaddir: service: %s\n", smbfs_info->conn->service));
	DEBUG(4,("smbfs_loaddir: cli->share: %s\n", smbfs_info->conn->cli->share));
	DEBUG(4,("smbfs_loaddir: calling cli_list with mask %s\n", my_dirname));
    if (cli_list(smbfs_info->conn->cli, my_dirname, attribute, loaddir_helper) < 1) {
		my_errno = ENOENT;
		return 0;	/* cli_list returns number of files */
	}
	if (strlen(my_dirname) == 0)
		smbfs_info->dirname = smbfs_info->conn->service;
	g_free(p);
/*    do_dskattr();	*/

done:
/*	current_info->parent = smbfs_info->dirname;	*/
    smbfs_info->current = smbfs_info->entries;
	return 1; /* 1 = ok */
}

static void smbfs_free_dir (dir_entry *de)
{
    if (!de) return;

    smbfs_free_dir (de->next);
    g_free (de->text);
    g_free (de);
}

/* Explanation:
 * On some operating systems (Slowaris 2 for example)
 * the d_name member is just a char long (Nice trick that break everything,
 * so we need to set up some space for the filename.
 */
static struct {
    struct dirent dent;
#ifdef NEED_EXTRA_DIRENT_BUFFER
    char extra_buffer [MC_MAXPATHLEN];
#endif
} smbfs_readdir_data;


/* The readdir routine loads the complete directory */
/* It's too slow to ask the server each time */
/* It now also sends the complete lstat information for each file */
static struct stat *cached_lstat_info;
static void *smbfs_readdir (void *info)
{
    char *dirent_dest;
	opendir_info  *smbfs_info = (opendir_info *) info;

	DEBUG(4,("smbfs_readdir(%s)\n", smbfs_info->dirname));
	
    if (!smbfs_info->entries)
	    if (!smbfs_loaddir (smbfs_info))
			return NULL;

    if (smbfs_info->current == 0) {	/* reached end of dir entries */
		DEBUG(4,("smbfs_readdir: smbfs_info->current = 0\n"));
	    cached_lstat_info = 0;
	    smbfs_free_dir (smbfs_info->entries);
	    smbfs_info->entries = 0;
	    return NULL;
    }
    dirent_dest = &(smbfs_readdir_data.dent.d_name [0]);
    pstrcpy (dirent_dest, smbfs_info->current->text);
    cached_lstat_info = &smbfs_info->current->my_stat;
    smbfs_info->current = smbfs_info->current->next;

#ifndef DIRENT_LENGTH_COMPUTED
    smbfs_readdir_data.dent.d_namlen = strlen (smbfs_readdir_data.dent.d_name);
#endif

    return &smbfs_readdir_data; 
}

static int smbfs_closedir (void *info)
{
    opendir_info *smbfs_info = (opendir_info *) info;
/*    dir_entry *p, *q;	*/

	DEBUG(4,("smbfs_closedir(%s)\n", smbfs_info->dirname));
	/*	CLOSE HERE */
    
/*    for (p = smbfs_info->entries; p;){
		q = p;
		p = p->next;
		g_free (q->text);
		g_free (q);
    }
    g_free (info);	*/
    return 0;
}

static int smbfs_chmod (vfs *me, char *path, int mode)
{
	DEBUG(4,("smbfs_chmod(path:%s, mode:%d)\n", path, mode));
/*	my_errno = EOPNOTSUPP;
	return -1;	*/	/* cant chmod on smb filesystem */
	return 0;		/* make mc happy */
}

static int smbfs_chown (vfs *me, char *path, int owner, int group)
{
	DEBUG(4,("smbfs_chown(path:%s, owner:%d, group:%d)\n", path, owner, group));
	my_errno = EOPNOTSUPP;	/* ready for your labotomy? */
	return -1;
}

static int smbfs_utime (vfs *me, char *path, struct utimbuf *times)
{
	DEBUG(4,("smbfs_utime(path:%s)\n", path));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int smbfs_readlink (vfs *me, char *path, char *buf, int size)
{
	DEBUG(4,("smbfs_readlink(path:%s, buf:%s, size:%d)\n", path, buf, size));
	my_errno = EOPNOTSUPP;
    return -1;	/* no symlinks on smb filesystem? */
}

static int smbfs_symlink (vfs *me, char *n1, char *n2)
{
	DEBUG(4,("smbfs_symlink(n1:%s, n2:%s)\n", n1, n2));
	my_errno = EOPNOTSUPP;
    return -1;	/* no symlinks on smb filesystem? */
}

/* Extract the hostname and username from the path */
/* path is in the form: hostname:user/remote-dir */
static char *smbfs_get_host_and_username (char **path, char **host, char **user,
                  int *port, char **pass)
{
	char *p, *ret;

    ret = vfs_split_url (*path, host, user, port, pass, SMB_PORT, 0);

	if ((p = strchr(*path, '@')))	/* user:pass@server */
		*path = ++p;				/* dont want user:pass@ in path */
	if ((p = strchr(ret, '@')))	/* user:pass@server */
		ret = ++p;				/* dont want user:pass@ in path */

	return ret;
}

/***************************************************** 
	return a connection to a SMB server
	current_bucket needs to be set before calling
*******************************************************/
struct cli_state *do_connect(char *server, char *share)
{
	struct cli_state *c;
	struct nmb_name called, calling;
	char *server_n;
	struct in_addr ip;
	extern struct in_addr ipzero;

	DEBUG(4,("do_connect(%s, %s)\n", server, share));
	if (*share == '\\') {
		server = share+2;
		share = strchr(server,'\\');
		if (!share) return NULL;
		*share = 0;
		share++;
	}

	server_n = server;
	
	ip = ipzero;

	make_nmb_name(&calling, global_myname, 0x0, "");
	make_nmb_name(&called , server, current_bucket->name_type, "");

 again:
	ip = ipzero;
	if (current_bucket->have_ip) ip = current_bucket->dest_ip;

	/* have to open a new connection */
	if (!(c=cli_initialise(NULL)) ||
			(cli_set_port(c, current_bucket->port) == 0) ||
			!cli_connect(c, server_n, &ip)) {
		DEBUG(0,("Connection to %s failed\n", server_n));
		my_errno = ENOTCONN;
		return NULL;
	}

	if (!cli_session_request(c, &calling, &called)) {
		DEBUG(0,("session request to %s failed\n", called.name));
		cli_shutdown(c);
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20, "");
			goto again;
		}
		my_errno = ENOTCONN;
		return NULL;
	}

	DEBUG(4,(" session request ok\n"));

	if (!cli_negprot(c)) {
		DEBUG(0,("protocol negotiation failed\n"));
		cli_shutdown(c);
		my_errno = ENOTCONN;
		return NULL;
	}

	if (!cli_session_setup(c, current_bucket->user, 
			       current_bucket->password, strlen(current_bucket->password),
			       current_bucket->password, strlen(current_bucket->password),
			       workgroup)) {
		DEBUG(1,("session setup failed: %s\n", cli_errstr(c)));
	    message_2s (1, MSG_ERROR, "session setup:%s ",
			cli_errstr(c));
		my_errno = ENOTCONN;
		return NULL;
	}

	/*
	 * These next two lines are needed to emulate
	 * old client behaviour for people who have
	 * scripts based on client output.
	 * QUESTION ? Do we want to have a 'client compatibility
	 * mode to turn these on/off ? JRA.
	 */

	if (*c->server_domain || *c->server_os || *c->server_type)
		DEBUG(1,("Domain=[%s] OS=[%s] Server=[%s]\n",
			c->server_domain,c->server_os,c->server_type));
	
	DEBUG(4,(" session setup ok\n"));

	if (!cli_send_tconX(c, share, "?????",
			    current_bucket->password, strlen(current_bucket->password)+1)) {
		DEBUG(1,("tree connect failed: %s\n", cli_errstr(c)));
	    message_2s (1, MSG_ERROR, "tconX:%s", cli_errstr(c));
		cli_shutdown(c);
		my_errno = ENOTCONN;
		return NULL;
	}

	DEBUG(4,(" tconx ok\n"));

	return c;
}

static int smbfs_get_free_bucket_init = 1;

static smbfs_connection *smbfs_get_free_bucket ()
{
    int i;
    
    if (smbfs_get_free_bucket_init) {
		smbfs_get_free_bucket_init = 0;
		for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++)
	    	smbfs_connections [i].host = 0;
    }
    for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++){
		if (!smbfs_connections [i].cli) return &smbfs_connections [i];
    }
    /* This can't happend, since we have checked for max connections before */
    vfs_die("Internal error: smbfs_get_free_bucket");
    return 0;	/* shut up, stupid gcc */
}

/* This routine keeps track of open connections */
/* Returns a connected socket to host */
static smbfs_connection *smbfs_open_link (char *host, char *path, char *user, int *port, char *this_pass)
{
    int i;
    smbfs_connection *bucket;
	pstring service;

	DEBUG(4,("smbfs_open_link(host:%s, path:%s)\n", host, path));

	if (strcmp(host, path) == 0)	/* if host & path are same: */
		pstrcpy(service, IPC);		/* setup for browse */
	else {	//get share name from path, path starts with server name
		char *p;
		if ((p = strchr(path, '/')))	/* get share aka				*/
			pstrcpy(service, ++p);	/* service name from path		*/

		/* now check for trailing directory/filenames	*/
		p = strchr(service, '/');
		if (p)
			*p = 0;				/* cut off dir/files: sharename only */

		DEBUG(4,("smbfs_open_link: service from path:%s\n", service));
	}

	if (got_user)
		user = g_strdup(username);	/* global from getenv */

    /* Is the link actually open? */
    if (smbfs_get_free_bucket_init) {
        smbfs_get_free_bucket_init = 0;
        for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++)
	    	smbfs_connections [i].host = 0;
    } else for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++) {
/*		if (!smbfs_connections [i].host)	*/
		if (!smbfs_connections[i].cli)
		    continue;
		if ((strcmp (host, smbfs_connections [i].host) == 0) &&
		    (strcmp (user, smbfs_connections [i].user) == 0) &&
		    (strcmp (service, smbfs_connections [i].service) == 0)) {
			DEBUG(4,("smbfs_open_link: returning smbfs_connection[%d]\n", i));
		    return &smbfs_connections [i];
		}
    }
    if (smbfs_open_connections == SMBFS_MAX_CONNECTIONS) {
		message_1s (1, MSG_ERROR, _(" Too many open connections "));
		return 0;
	}

	/* make new connection */
    bucket = smbfs_get_free_bucket ();
	bucket->name_type = 0x20;
    bucket->home = 0;
	bucket->port = *port;
	current_bucket = bucket;

	bucket->user = g_strdup(user);
	if (got_pass)
		pstrcpy(bucket->password, password);	/* global from getenv */
	else
		pstrcpy(bucket->password, this_pass);

	/* do connect from smbclient takes us into beloved smb land */
    if (!(bucket->cli = do_connect(host, service)))
		return 0;

	/* successfull connection, finish the connection bucket */
	pstrcpy(bucket->service, service);
	bucket->host = g_strdup(host);
//	pstrcpy(bucket->user, user);
    
    smbfs_open_connections++;
    DEBUG(4,("smbfs_open_link:smbfs_open_connections: %d\n", smbfs_open_connections));
    return bucket;
}

char *strip(char *path) {   /* use as "strcat(path, strip(path))" */
    char *x, *y;
    x = strchr(path, '/');
    y = strchr(path, '~');
    if (x && y && (y-x) == 1) { /* strip off /~ */
        y++;
        *x = 0;
        return y;
    }
    return "";
}

static char *smbfs_get_path(smbfs_connection **sc, char *path) {
	char *user, *host, *remote_path, *pass;
	int port = SMB_PORT;

	DEBUG(4,("smbfs_get_path(%s)\n", path));
    if (strncmp (path, "/#smb:", 6))
        return NULL;
    path += 6;

	pstrcat(path, strip(path));

	if ((remote_path = smbfs_get_host_and_username(
		&path, &host, &user, &port, &pass)))  
		if ((*sc = smbfs_open_link (host, path, user, &port, pass)) == NULL){
			g_free (remote_path);
			remote_path = NULL;
		}
    g_free (host);
    g_free (user);
    if (pass) wipe_password (pass);

    if (!remote_path) return NULL;

    /* NOTE: tildes are deprecated. See ftpfs.c */
    {
        int f = !strcmp( remote_path, "/~" );
	    if (f || !strncmp( remote_path, "/~/", 3 )) {
			char *s;
	        s = concat_dir_and_file( (*sc)->home, remote_path +3-f );
			g_free (remote_path);
			remote_path = s;
		}
	}
	return remote_path;
}

static int is_error (int result, int errno_num)
{
    if (!(result == -1))
		return my_errno = 0;
    else 
		my_errno = errno_num;
    return 1;
}

static void *smbfs_opendir (vfs *me, char *dirname)
{
    opendir_info *smbfs_info;
	smbfs_connection *sc;
	char *remote_dir;

	DEBUG(4,("smbfs_opendir(dirname:%s)\n", dirname));

    if (!(remote_dir = smbfs_get_path (&sc, dirname)))
		return NULL;

    smbfs_info = g_new (opendir_info, 1);
	smbfs_info->dirname = remote_dir;
    smbfs_info->conn = sc;
    smbfs_info->entries = 0;
    smbfs_info->current = 0;

    return smbfs_info;
}

static int fake_share_stat(char *server_url, char *path, struct stat *buf) {
//	pstring my_path;
	dir_entry *dentry;
	path += strlen(server_url);	/*	we only want share name	*/
	pstrcat(path, strip(path));		/* strip off /~ */
	path++;
	if (!current_info->entries) {
//		pstrcpy(my_path, path);
		if (!smbfs_loaddir(current_info));	/* browse host */
			return -1;
//		path = g_strdup(path);
	}
	dentry = current_info->entries;
	DEBUG(4,("smbfs_stat: fake stat for share %s\n", path));
	while (dentry) {
		if (strcmp(dentry->text, path) == 0) {
			DEBUG(4,("smbfs_stat: %s:%d\n",
				dentry->text, dentry->my_stat.st_mode));
			memcpy(buf, &dentry->my_stat, sizeof(struct stat));
			if (S_ISDIR (buf->st_mode))	DEBUG(4,("<S_ISDIR>\n"));
			return 0;
		}
		dentry = dentry->next;
	}
	my_errno = ENOENT;
	return -1;
}

/* stat a single file, get_remote_stat callback  */
dir_entry *single_entry;
static void statfile_helper(file_info *finfo, const char *mask) {
	time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */
	single_entry = g_new (dir_entry, 1);
    single_entry->text = g_new0 (char, strlen(finfo->name)+1);
	pstrcpy(single_entry->text, finfo->name);

    single_entry->next = 0;
	single_entry->my_stat.st_size = finfo->size;
	single_entry->my_stat.st_mtime = finfo->mtime;
	single_entry->my_stat.st_atime = finfo->atime;
	single_entry->my_stat.st_ctime = finfo->ctime;
	single_entry->my_stat.st_uid = finfo->uid;
	single_entry->my_stat.st_gid = finfo->gid;

//	new_entry->my_stat.st_mode = finfo->mode;
	single_entry->my_stat.st_mode =	S_IRUSR|S_IRGRP|S_IROTH |
									S_IWUSR|S_IWGRP|S_IWOTH;
								
/*  if (finfo->mode & aVOLID);	 nothing similar in real world */
	if (finfo->mode & aDIR)
		single_entry->my_stat.st_mode |= S_IFDIR;
	else
		single_entry->my_stat.st_mode |= S_IFREG;/* if not dir, regular file? */
/*  if (finfo->mode & aARCH);	DOS archive	*/
/*  if (finfo->mode & aHIDDEN);	like a dot file? */
/*  if (finfo->mode & aSYSTEM); like a kernel? */
  if (finfo->mode & aRONLY)
		single_entry->my_stat.st_mode &= ~(S_IRUSR|S_IRGRP|S_IROTH);
 
	DEBUG(4, ("  %-30s%7.7s%8.0f  %s",
		CNV_LANG(finfo->name),
		attrib_string(finfo->mode),
		(double)finfo->size,
		asctime(LocalTime(&t)))); 
}

/* stat a single file */
static int get_remote_stat(smbfs_connection *sc, char *path, struct stat *buf) {
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	char *mypath = g_new(char, sizeof(path)+1);
	mypath = g_strdup(path);

	DEBUG(4, ("get_remote_stat(): mypath:%s\n", mypath));

	convert_path(&mypath, FALSE);

    if (cli_list(sc->cli, mypath, attribute, statfile_helper) < 1) {
		my_errno = ENOENT;
		return -1;	/* cli_list returns number of files */
	}

	memcpy(buf, &single_entry->my_stat, sizeof(struct stat));

/* dont free here, use for smbfs_fstat() */
/*	g_free(single_entry->text);
	g_free(single_entry);	*/
	return 0;
}

static int get_stat_info (smbfs_connection *sc, char *path, struct stat *buf) {
	char *p, *mpp;
	dir_entry *dentry = current_info->entries;
	char *mypath = g_new(char, strlen(path)+1);
	mpp = mypath = g_strdup(path);

	mypath++;				/* cut off leading '/' */
	while ((p = strchr(mypath, '/')))
		mypath++;			/* advance until last file/dir name */
	DEBUG(4, ("get_stat_info: mypath:%s, current_info->dirname:%s\n",
		mypath, current_info->dirname));
	while (dentry) {
//		DEBUG(4,("get_stat_info: dentry->text:%s\n", dentry->text));
		if (strcmp(mypath, dentry->text) == 0) {
			memcpy(buf, &dentry->my_stat, sizeof(struct stat));
			g_free(mpp);
			return 0;
		}
		dentry = dentry->next;
	}
		/* now try to identify dentry->text as parent dir */
	{
		char *mdp;
		char *mydir = g_new(char, strlen(current_info->dirname)+1);
		mdp = mydir = g_strdup(current_info->dirname);
		while ((p = strchr(mydir, '/')))
			mydir++;				/* advance util last '/' */
		mydir--;
		*mydir = 0;					/* cut off last name	*/
		mydir = mdp;				/* reset to start */
		while ((p = strchr(mydir, '/')))
			mydir++;				/* advance util last '/' */
		if (strcmp(mydir, mypath) == 0) {	/* fake a stat for ".." */
			struct stat fake_stat;
			bzero(&fake_stat, sizeof(struct stat));
			fake_stat.st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
			memcpy(buf, &fake_stat, sizeof(struct stat));
			g_free(mdp);
			g_free(mpp);
			return 0;
		}
		g_free(mdp);
	}
	g_free(mpp);
	/* nothing found. get stat file info from server */
	return get_remote_stat(sc, path, buf);
}

static int smbfs_stat (vfs *me, char *path, struct stat *buf)
{
	char *remote_dir;
	smbfs_connection *sc;
	pstring server_url;

	DEBUG(4,("smbfs_stat(path:%s)\n", path));
	pstrcpy(server_url, "/#smb:");
	pstrcat(server_url, current_bucket->host);
//	DEBUG(4,("smbfs_stat: server_url:%s\n", server_url));

#if 0
	if (p = strchr(path, '@'))	/* user:pass@server */
		path = ++p;				/* dont want user:pass@ in path */
#endif

	/* should check for /#smb:server/share in path insead */
	/*	wants to stat shares, need to fake	*/
	if (strcmp(current_info->dirname, "/") == 0)
		return fake_share_stat(server_url, path, buf);

    if (!(remote_dir = smbfs_get_path (&sc, path))) /* connects if necessary */
		return -1;

	if (strcmp(server_url, path) == 0) {
		/* make server name appear as directory */
		DEBUG(4,("smbfs_stat: showing server as directory\n"));
		bzero(buf, sizeof(struct stat));
		buf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
    	return 0;
	}
	/* stat dirs & files under shares now */
	return get_stat_info(sc, path, buf);
}

static int smbfs_lstat (vfs *me, char *path, struct stat *buf)
{
	DEBUG(4,("smbfs_lstat(path:%s)\n", path));
	return smbfs_stat(me, path, buf);	/* no symlinks on smb filesystem? */
}

static int smbfs_chdir (vfs *me, char *path)
{
	char *remote_dir;
	smbfs_connection *sc;
	
	DEBUG(4,("smbfs_chdir(path:%s)\n", path));
    if (!(remote_dir = smbfs_get_path (&sc, path)))
		return -1;
	
    return 0;
}

static int smbfs_lseek (void *data, off_t offset, int whence)
{
	DEBUG(4,("smbfs_lseek()\n"));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int smbfs_mknod (vfs *me, char *path, int mode, int dev)
{
	DEBUG(4,("smbfs_mknod(path:%s, mode:%d, dev:%d)\n", path, mode, dev));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int smbfs_mkdir (vfs *me, char *path, mode_t mode)
{
	smbfs_connection *sc;
	char *remote_file;

	DEBUG(4,("smbfs_mkdir(path:%s, mode:%d)\n", path, mode));
	if ((remote_file = smbfs_get_path (&sc, path)) == 0)
		return -1;
 
	convert_path(&path, FALSE);

	if (!cli_mkdir(sc->cli, path)) {
        message_3s (1, MSG_ERROR, _(" %s mkdir'ing %s "), 
			cli_errstr(sc->cli), CNV_LANG(path));
		return -1;
	} 
    return 0;
}

static int smbfs_rmdir (vfs *me, char *path)
{
	smbfs_connection *sc;
	char *remote_file;

	DEBUG(4,("smbfs_rmdir(path:%s)\n", path));
	if ((remote_file = smbfs_get_path (&sc, path)) == 0)
		return -1;

	convert_path(&path, FALSE);

	if (!cli_rmdir(sc->cli, path)) {
        message_3s (1, MSG_ERROR, _(" %s rmdir'ing %s "), 
			cli_errstr(sc->cli), CNV_LANG(path));
		return -1;
	} 
    return 0;
}

static int smbfs_link (vfs *me, char *p1, char *p2)
{
	DEBUG(4,("smbfs_link(p1:%s, p2:%s)\n", p1, p2));
	my_errno = EOPNOTSUPP;
    return -1;
}

/* We do not free anything right now: we free resources when we run
 * out of them
 */
static vfsid smbfs_getid (vfs *me, char *p, struct vfs_stamping **parent)
{
    *parent = NULL;
	DEBUG(4,("smbfs_getid(p:%s)\n", p));
    
    return (vfsid) -1;
}

static int smbfs_nothingisopen (vfsid id)
{
	DEBUG(4,("smbfs_nothingisopen(%d)\n", (int)id));
    return 0;
}

static void smbfs_free (vfsid id)
{
	DEBUG(4,("smbfs_free(%d)\n", (int)id));
    /* FIXME: Should not be empty */
}

/* Gives up on a socket and reopnes the connection, the child own the socket
 * now
 */
static void
my_forget (char *path)
{
    char *host, *user, *pass, *p;
    int  port, i;

    if (strncmp (path, "/#smb:", 6))
		return;

	DEBUG(4, ("my_forget(path:%s)\n", path));
    
    path += 6;
    if (path[0] == '/' && path[1] == '/')
	path += 2;

    if ((p = smbfs_get_host_and_username (&path, &host, &user, &port, &pass))
		== 0) {
		g_free (host);
		g_free (user);
		if (pass)
		    wipe_password (pass);
		return;
    }
    for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++) {
		if ((strcmp (host, smbfs_connections [i].host) == 0) &&
		    (strcmp (user, smbfs_connections [i].user) == 0) &&
		    (port == smbfs_connections [i].port)) {

		    /* close socket: the child owns it now */
			cli_shutdown(smbfs_connections [i].cli);

		    /* reopen the connection */
		    smbfs_connections [i].cli =
				do_connect(host, smbfs_connections[i].service);
		}
    }
    g_free (p);
    g_free (host);
    g_free (user);
    if (pass)
		wipe_password (pass);
}

static int 
smbfs_setctl (vfs *me, char *path, int ctlop, char *arg)
{
	DEBUG(4,("smbfs_setctl(path:%s, ctlop:%d)\n", path, ctlop));
    switch (ctlop) {
        case MCCTL_FORGET_ABOUT:
	    my_forget(path);
	    return 0;
    }
    return 0;
}

static smbfs_handle *open_write (smbfs_handle *remote_handle, char *rname,
	int flags, int mode) {
	if (flags & O_TRUNC)	/* if it exists truncate to zero */
		DEBUG(4, ("open_write: O_TRUNC\n"));

	remote_handle->fnum = cli_open(remote_handle->cli, rname, flags, DENY_NONE);

	if (remote_handle->fnum == -1) {
        message_3s (1, MSG_ERROR, _(" %s opening remote file %s "), 
			cli_errstr(remote_handle->cli), CNV_LANG(rname));
		DEBUG(1,("smbfs_open(rname:%s) error:%s\n",
			rname, cli_errstr(remote_handle->cli)));
		my_errno = EIO;
    	return NULL;
	}
	
    return remote_handle;
}

static smbfs_handle *open_read (smbfs_handle *remote_handle, char *rname,
	int flags, int mode) {
	size_t		size;

	remote_handle->fnum =
		cli_open(remote_handle->cli, rname, O_RDONLY, DENY_NONE);

	if (remote_handle->fnum == -1) {
        message_3s (1, MSG_ERROR, _(" %s opening remote file %s "), 
			cli_errstr(remote_handle->cli), CNV_LANG(rname));
		DEBUG(1,("smbfs_open(rname:%s) error:%s\n",
			rname, cli_errstr(remote_handle->cli)));
		my_errno = EIO;
    	return NULL;
	}
 
    if (!cli_qfileinfo(remote_handle->cli, remote_handle->fnum,
               &remote_handle->attr, &size, NULL, NULL, NULL, NULL, NULL) &&
        !cli_getattrE(remote_handle->cli, remote_handle->fnum,
              &remote_handle->attr, &size, NULL, NULL, NULL)) {
	    message_2s (1, MSG_ERROR, " getattrib: %s ",
			cli_errstr(remote_handle->cli));
		DEBUG(1,("smbfs_open(rname:%s) getattrib:%s\n",
			rname, cli_errstr(remote_handle->cli)));
		my_errno = EBADF;	/* just something unique & indentifiable */
        return NULL;
    }

    return remote_handle;
}

static void *smbfs_open (vfs *me, char *file, int flags, int mode)
{
    char *remote_file;
	void *ret;
    smbfs_connection	*sc;
    smbfs_handle		*remote_handle;

	DEBUG(4,("smbfs_open(file:%s, flags:%d, mode:%d)\n", file, flags, mode));

    if (!(remote_file = smbfs_get_path (&sc, file)))
		return 0;

	convert_path(&remote_file, FALSE);
//	DEBUG(4,("smbfs_open(remote_file:%s)\n", remote_file));

    remote_handle			= g_new (smbfs_handle, 2);
    remote_handle->cli		= sc->cli;
	remote_handle->nread	= 0;

	if (flags & O_CREAT)
		ret = open_write(remote_handle, remote_file, flags, mode);
	else
		ret = open_read(remote_handle, remote_file, flags, mode);
	return ret;
}

static int smbfs_unlink (vfs *me, char *path)
{
    smbfs_connection *sc;
	char *remote_file;

    if ((remote_file = smbfs_get_path (&sc, path)) == 0)
	    return -1;
 
	convert_path(&remote_file, FALSE);

    if (!cli_unlink(sc->cli, remote_file)) {
        message_3s (1, MSG_ERROR, _(" %s opening remote file %s "), 
			cli_errstr(sc->cli), CNV_LANG(remote_file));
		return -1;
    }   
    return 0;
}

static int smbfs_rename (vfs *me, char *a, char *b)
{
    smbfs_connection *sc;
	char *ra, *rb;

    if ((ra = smbfs_get_path (&sc, a)) == 0)
	    return -1;
 
    if ((rb = smbfs_get_path (&sc, b)) == 0)
	    return -1;
 
	convert_path(&ra, FALSE);
	convert_path(&rb, FALSE);

    if (!cli_rename(sc->cli, ra, rb)) {
        message_2s (1, MSG_ERROR, _(" %s renaming files\n"), 
			cli_errstr(sc->cli));
		return -1;
    }   
    return 0;
}

static int smbfs_fstat (void *data, struct stat *buf)
{
    smbfs_handle *remote_handle = (smbfs_handle *)data;

	DEBUG(4,("smbfs_fstat(fnum:%d)\n", remote_handle->fnum));

	/* use left over from previous get_remote_stat, if available */
	if (single_entry)
		memcpy(buf, &single_entry->my_stat, sizeof(struct stat));
/*	my_errno = EOPNOTSUPP;
	return -1;	*/
	return 0;
}

vfs vfs_smbfs_ops = {
    NULL,	/* This is place of next pointer */
    "netbios over tcp/ip",
    F_NET,	/* flags */
    "smb:",	/* prefix */
    NULL,	/* data */
    0,		/* errno */
    smbfs_init,
    NULL,
    smbfs_fill_names,
    NULL,	//which

    smbfs_open,
    smbfs_close,
    smbfs_read,
    smbfs_write,
    
    smbfs_opendir,
    smbfs_readdir,
    smbfs_closedir,
    NULL,
    NULL,

    smbfs_stat,
    smbfs_lstat,
    smbfs_fstat,

    smbfs_chmod,
    smbfs_chown,
    smbfs_utime,

    smbfs_readlink,
    smbfs_symlink,
    smbfs_link,
    smbfs_unlink,

    smbfs_rename,
    smbfs_chdir,
    smbfs_errno,
    smbfs_lseek,
    smbfs_mknod,
    
    smbfs_getid,
    smbfs_nothingisopen,
    smbfs_free,
    
    NULL,
    NULL,

    smbfs_mkdir,
    smbfs_rmdir,
    NULL,
    smbfs_setctl

MMAPNULL
};
