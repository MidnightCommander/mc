/* Virtual File System: Midnight Commander file system.
   
   Copyright (C) 1995, 1996, 1997 The Free Software Foundation

   Written by Wayne Roberts
    <wroberts1@home.com>
   
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
#include <stdio.h>
#include <sys/types.h>

#include "../config.h"
#include "samba/include/config.h"
/* don't load crap in "samba/include/includes.h" we don't use and which 
   confilcts with definitions in other includes */
#undef HAVE_LIBREADLINE
#define NO_CONFIG_H
#include "samba/include/includes.h"

#include <string.h>
#include <errno.h>

#include "../src/global.h"

#include "../src/tty.h"         /* enable/disable interrupt key */
#include "../src/main.h"

#include "vfs.h"
#include "smbfs.h"
#include "tcputil.h"
#include "../src/dialog.h"
#include "../src/widget.h"
#include "../src/color.h"
#include "../src/wtools.h"

#define SMBFS_MAX_CONNECTIONS 16
char *IPC = "IPC$";
char *URL_HEADER = "/#smb:";
#define HEADER_LEN	6

static int my_errno;
uint32 err;

/* stuff that is same with each connection */
extern int DEBUGLEVEL;
static pstring myhostname;
mode_t myumask = 0755;
extern pstring global_myname;
static int smbfs_open_connections = 0;
gboolean got_user = FALSE;
gboolean got_pass = FALSE;
pstring password;
pstring username;

static struct _smbfs_connection {
	struct cli_state *cli;
	struct in_addr dest_ip;
	BOOL have_ip;
	char *host;			/* server name */
	char *service;			/* share name */
	char *domain;
	char *user;
	char *home;
	char *password;
	int port;
	int name_type;
	time_t	last_use;
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

struct authinfo {
    char *host;
    char *share;
    char *domain;
    char *user;
    char *password;
/*	struct timeval timestamp;*/
};

GSList *auth_list;

static struct authinfo *
authinfo_get_authinfo_from_user (const char *host, 
                                 const char *share, 
                                 const char *domain, 
                                 const char *user)
{
    static int dialog_x = 44;
    int dialog_y = 12;
    struct authinfo *return_value;
    static char* labs[] = {N_("Domain:"), N_("Username:"), N_("Password: ")};
    static char* buts[] = {N_("&Ok"), N_("&Cancel")};
    static int ilen = 30, istart = 14;
    static int b0 = 3, b2 = 30;
    char *title;
    WInput *in_password;
    WInput *in_user;
    WInput *in_domain;
    Dlg_head *auth_dlg;

#ifdef ENABLE_NLS
    static int i18n_flag = 0;
    
    if (!i18n_flag)
    {
        register int i = sizeof(labs)/sizeof(labs[0]);
        int l1, maxlen = 0;
        
        while (i--)
        {
            l1 = strlen (labs [i] = _(labs [i]));
            if (l1 > maxlen)
                maxlen = l1;
        }
        i = maxlen + ilen + 7;
        if (i > dialog_x)
            dialog_x = i;
        
        for (i = sizeof(buts)/sizeof(buts[0]), l1 = 0; i--; )
        {
            l1 += strlen (buts [i] = _(buts [i]));
        }
        l1 += 15;
        if (l1 > dialog_x)
            dialog_x = l1;

        ilen = dialog_x - 7 - maxlen; /* for the case of very long buttons :) */
        istart = dialog_x - 3 - ilen;
        
        b2 = dialog_x - (strlen(buts[1]) + 6);
        
        i18n_flag = 1;
    }
    
#endif /* ENABLE_NLS */

    if (!domain)
    domain = "";
    if (!user)
    user = "";

    auth_dlg = create_dlg (0, 0, dialog_y, dialog_x, dialog_colors,
                       common_dialog_callback, "[Smb Authinfo]", "smbauthinfo",
                       DLG_CENTER | DLG_GRID);

    title = g_strdup_printf (_("Password for \\\\%s\\%s"), host, share);
    x_set_dialog_title (auth_dlg, title);
    g_free (title);

    in_user  = input_new (5, istart, INPUT_COLOR, ilen, user, "auth_name");
    add_widgetl (auth_dlg, in_user, XV_WLAY_BELOWOF);

    in_domain = input_new (3, istart, INPUT_COLOR, ilen, domain, "auth_domain");
    add_widgetl (auth_dlg, in_domain, XV_WLAY_NEXTCOLUMN);
    add_widgetl (auth_dlg, button_new (9, b2, B_CANCEL, NORMAL_BUTTON,
                 buts[1], 0 ,0, "cancel"), XV_WLAY_RIGHTOF);
    add_widgetl (auth_dlg, button_new (9, b0, B_ENTER, DEFPUSH_BUTTON,
                 buts[0], 0, 0, "ok"), XV_WLAY_CENTERROW);

    in_password  = input_new (7, istart, INPUT_COLOR, ilen, "", "auth_password");
    in_password->completion_flags = 0;
    in_password->is_password = 1;
    add_widgetl (auth_dlg, in_password, XV_WLAY_BELOWOF);

    add_widgetl (auth_dlg, label_new (7, 3, labs[2], "label-passwd"), XV_WLAY_BELOWOF);
    add_widgetl (auth_dlg, label_new (5, 3, labs[1], "label-user"), XV_WLAY_BELOWOF);
    add_widgetl (auth_dlg, label_new (3, 3, labs[0], "label-domain"), XV_WLAY_NEXTCOLUMN);

    run_dlg (auth_dlg);

    switch (auth_dlg->ret_value) {
    case B_CANCEL:
        return_value = 0;
        break;
    default:
        return_value = g_new (struct authinfo, 1);
        if (return_value) {
            return_value->host = g_strdup (host);
            return_value->share = g_strdup (share);
            return_value->domain = g_strdup (in_domain->buffer);
            return_value->user = g_strdup (in_user->buffer);
            return_value->password = g_strdup (in_password->buffer);
        }
    }

    destroy_dlg (auth_dlg);
             
    return return_value;
}

static void 
authinfo_free (struct authinfo const *a)
{
    g_free (a->host);
    g_free (a->share);
    g_free (a->domain);
    g_free (a->user);
    wipe_password (a->password);
}

static void
authinfo_free_all ()
{
    if (auth_list) {
        g_slist_foreach (auth_list, (GFunc)authinfo_free, 0);
        g_slist_free (auth_list);
        auth_list = 0;
    }
}

static gint
authinfo_compare_host_and_share (gconstpointer _a, gconstpointer _b)
{
    struct authinfo const *a = (struct authinfo const *)_a;
    struct authinfo const *b = (struct authinfo const *)_b;

    if (!a->host || !a->share || !b->host || !b->share)
        return 1;
    if (strcmp (a->host, b->host) != 0)
        return 1;
    if (strcmp (a->share, b->share) != 0)
        return 1;
    return 0;
}

static gint
authinfo_compare_host (gconstpointer _a, gconstpointer _b)
{
    struct authinfo const *a = (struct authinfo const *)_a;
    struct authinfo const *b = (struct authinfo const *)_b;

    if (!a->host || !b->host)
        return 1;
    if (strcmp (a->host, b->host) != 0)
        return 1;
    if (strcmp (a->share, IPC) != 0)
        return 1;
    return 0;
}

static void
authinfo_add (const char *host, const char *share, const char *domain,
              const char *user, const char *password)
{
    struct authinfo *auth = g_new (struct authinfo, 1);
    
    if (!auth)
        return;

    /* Don't check for NULL, g_strdup already does. */
    auth->host = g_strdup (host);
    auth->share = g_strdup (share);
    auth->domain = g_strdup (domain);
    auth->user = g_strdup (user);
    auth->password = g_strdup (password);
    auth_list = g_slist_prepend (auth_list, auth);
}

static void
authinfo_remove (const char *host, const char *share)
{
    struct authinfo data;
    struct authinfo *auth;
    GSList *list;

    data.host = g_strdup (host);
    data.share = g_strdup (share);
    list = g_slist_find_custom (auth_list, 
                                &data, 
                                authinfo_compare_host_and_share);
    g_free (data.host);
    g_free (data.share);
    if (!list)
        return;
    auth = list->data;
    auth_list = g_slist_remove (auth_list, auth);
    authinfo_free (auth);
}

/* Set authentication information in bucket. Return 1 if successful, else 0 */
/* Information in auth_list overrides user if pass is NULL. */
/* bucket->host and bucket->service must be valid. */
static int
bucket_set_authinfo (smbfs_connection *bucket, 
        const char *domain, const char *user, const char *pass,
        int fallback_to_host)
{
    struct authinfo data;
    struct authinfo *auth;
    GSList *list;

    if (domain && user && pass) {
        g_free (bucket->domain);
        g_free (bucket->user);
        g_free (bucket->password);
        bucket->domain = g_strdup (domain);
        bucket->user = g_strdup (user);
        bucket->password = g_strdup (pass);
        authinfo_remove (bucket->host, bucket->service);
        authinfo_add (bucket->host, bucket->service,
                  domain, user, pass);
        return 1;
    }

    data.host = bucket->host;
    data.share = bucket->service;
    list = g_slist_find_custom (auth_list, &data, authinfo_compare_host_and_share);
    if (!list && fallback_to_host)
        list = g_slist_find_custom (auth_list, &data, authinfo_compare_host);
    if (list) {
        auth = list->data;
        bucket->domain = g_strdup (auth->domain);
        bucket->user = g_strdup (auth->user);
        bucket->password = g_strdup (auth->password);
        return 1;
    }

    if (got_pass) {
        bucket->domain = g_strdup (lp_workgroup ());
        bucket->user = g_strdup (got_user ? username : user);
        bucket->password = g_strdup (password);
        return 1;
    }

    auth = authinfo_get_authinfo_from_user (bucket->host, 
                                bucket->service,
                                (domain ? domain : lp_workgroup ()),
                                user);
    if (auth) {
        g_free (bucket->domain);
        g_free (bucket->user);
        g_free (bucket->password);
        bucket->domain = g_strdup (auth->domain);
        bucket->user = g_strdup (auth->user);
        bucket->password = g_strdup (auth->password);
        authinfo_remove (bucket->host, bucket->service);
        auth_list = g_slist_prepend (auth_list, auth);
        return 1;
    }
    return 0;
}

void
smbfs_set_debug(int arg)
{
	DEBUGLEVEL = arg;
}

/********************** The callbacks ******************************/
static int
smbfs_init(vfs *me)
{
	char *servicesf = "/etc/smb.conf";

/*	DEBUGLEVEL = 4;	*/

	setup_logging("mc", True);
    TimeInit();
    charset_initialise();

	DEBUG(3, ("smbfs_init(%s)\n", me->name));

	if(!get_myname(myhostname,NULL))
        DEBUG(0,("Failed to get my hostname.\n"));

	if (!lp_load(servicesf,True,False,False))
		DEBUG(0, ("Can't load %s - run testparm to debug it\n", servicesf));

    codepage_initialise(lp_client_code_page());

    load_interfaces();
    myumask = umask(0);
    umask(myumask);

    if (getenv("USER")) {
		char *p;
        pstrcpy(username, getenv("USER"));
		got_user = TRUE;
		DEBUG(3, ("smbfs_init(): $USER:%s\n", username));
        if ((p = strchr(username, '%'))) {
            *p = 0;
            pstrcpy(password, p+1);
            got_pass = TRUE;
            memset(strchr(getenv("USER"), '%')+1, 'X', strlen(password));
			DEBUG(3, ("smbfs_init(): $USER%%pass: %s%%%s\n",
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

static void
smbfs_fill_names (vfs *me, void (*func)(char *))
{
	DEBUG(3, ("smbfs_fill_names()\n"));
}

#define CNV_LANG(s) dos_to_unix(s,False)
#define GNAL_VNC(s) unix_to_dos(s,False)
/* does same as do_get() in client.c */
/* called from vfs.c:1080, count = buffer size */
static int
smbfs_read (void *data, char *buffer, int count)
{
    smbfs_handle *info = (smbfs_handle *) data;
	int n;

	DEBUG(3, ("smbfs_read(fnum:%d, nread:%d, count:%d)\n",
		info->fnum, (int)info->nread, count));
	n = cli_read(info->cli, info->fnum, buffer, info->nread, count);
	if (n > 0)
		info->nread += n;
	return n;
}

static int
smbfs_write (void *data, char *buf, int nbyte)
{
    smbfs_handle *info = (smbfs_handle *) data;
	int n;

	DEBUG(3, ("smbfs_write(fnum:%d, nread:%d, nbyte:%d)\n",
		info->fnum, (int)info->nread, nbyte));
	n = cli_write(info->cli, info->fnum, 0, buf, info->nread, nbyte);
	if (n > 0)
		info->nread += n;
    return n;
}

static int
smbfs_close (void *data)
{
    smbfs_handle *info = (smbfs_handle *) data;
	DEBUG(3, ("smbfs_close(fnum:%d)\n", info->fnum));
/* if imlementing archive_level:	add rname to smbfs_handle
    if (archive_level >= 2 && (inf->attr & aARCH)) {
        cli_setatr(info->cli, rname, info->attr & ~(uint16)aARCH, 0);
    }	*/
	return cli_close(info->cli, info->fnum);
}

static int
smbfs_errno (vfs *me)
{
	DEBUG(3, ("smbfs_errno: %s\n", g_strerror(my_errno)));
    return my_errno;
}

typedef struct dir_entry {
    char *text;
    struct dir_entry *next;
    struct stat my_stat;
    int    merrno;
} dir_entry;

typedef struct {
	gboolean server_list;
	char *dirname;
	char *path;	/* the dir orginally passed to smbfs_opendir */
    smbfs_connection *conn;
    dir_entry *entries;
    dir_entry *current;
} opendir_info;

opendir_info
	*previous_info,
	*current_info,
	*current_share_info,
	*current_server_info;

gboolean first_direntry;
/* browse for shares on server */
static void
browsing_helper(const char *name, uint32 type, const char *comment)
{
	fstring typestr;
	dir_entry *new_entry;
	new_entry = g_new (dir_entry, 1);
    new_entry->text = dos_to_unix (g_strdup (name), 1);

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
			/*	show this as dir	*/
			new_entry->my_stat.st_mode = S_IFDIR|S_IRUSR|S_IRGRP|S_IROTH;
			break;
		case STYPE_PRINTQ:
			fstrcpy(typestr,"Printer"); break;
		case STYPE_DEVICE:
			fstrcpy(typestr,"Device"); break;
		case STYPE_IPC:
			fstrcpy(typestr, "IPC"); break;
	}
	DEBUG(3, ("\t%-15.15s%-10.10s%s\n", name, typestr,comment)); 
}

static void
loaddir_helper(file_info *finfo, const char *mask)
{
	dir_entry *new_entry;
	time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */

	if (finfo->mode & aHIDDEN)
		return;	/* dont bother with hidden files, "~$" screws up mc */

	new_entry = g_new (dir_entry, 1);
    new_entry->text = dos_to_unix (g_strdup(finfo->name), 1);

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
	new_entry->my_stat.st_size = finfo->size;
	new_entry->my_stat.st_mtime = finfo->mtime;
	new_entry->my_stat.st_atime = finfo->atime;
	new_entry->my_stat.st_ctime = finfo->ctime;
	new_entry->my_stat.st_uid = finfo->uid;
	new_entry->my_stat.st_gid = finfo->gid;


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
 
	DEBUG(3, ("  %-30s%7.7s%8.0f  %s",
		CNV_LANG(finfo->name),
		attrib_string(finfo->mode),
		(double)finfo->size,
		asctime(LocalTime(&t)))); 
}

/* takes "/foo/bar/file" and gives "\\foo\\bar\\file" */
static int
convert_path(char **remote_file, gboolean trailing_asterik)
{
	char *p, *my_remote;

	my_remote = *remote_file;
    if (strncmp (my_remote, URL_HEADER, HEADER_LEN) == 0) {	/* if passed directly */
		my_remote += 6;
		if (*my_remote == '/')		/* from server browsing */
			my_remote++;
		p = strchr(my_remote, '/');
		if (p)
			my_remote = p+1;	/* advance to end of server name */
	}

    if (*my_remote == '/')
        my_remote++;  /* strip off leading '/' */
    p = strchr(my_remote, '/');
    if (p)
        my_remote = p;   /* strip off share/service name */
    /* create remote filename as understood by smb clientgen */
    p = *remote_file = g_strconcat (my_remote, 
							        trailing_asterik ? "/*" : "", 
								    0);
    unix_to_dos (*remote_file, 1);
    while ((p = strchr(p, '/')))
        *p = '\\';
	return 0;
}

static void
server_browsing_helper(const char *name, uint32 m, const char *comment)
{
	dir_entry *new_entry;
	new_entry = g_new (dir_entry, 1);
    new_entry->text = dos_to_unix (g_strdup (name), 1);

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

	/*	show this as dir	*/
	new_entry->my_stat.st_mode = S_IFDIR|S_IRUSR|S_IRGRP|S_IROTH;

	DEBUG(3, ("\t%-16.16s     %s\n", name, comment));
}

static BOOL
reconnect(smbfs_connection *conn, int *retries) {
	char *host;
	DEBUG(3, ("RECONNECT\n"));

	if (strlen(conn->host) == 0)
		host = g_strdup(conn->cli->desthost);		/* server browsing */
	else
		host = g_strdup(conn->host);

	cli_shutdown(conn->cli);

   	if (!(conn->cli = do_connect(host, conn->service))) {
		my_errno = cli_error(conn->cli, NULL, &err, NULL);
		message_2s (1, MSG_ERROR,
			_(" reconnect to %s failed\n "), conn->host);
		g_free(host);
		return False;
	}
	g_free(host);
	if (++(*retries) == 2)
		return False;
	return True;
}

static BOOL
smb_send(struct cli_state *cli)
{
	size_t len;
	size_t nwritten=0;
	ssize_t ret;

	len = smb_len(cli->outbuf) + 4;

	while (nwritten < len) {
		ret = write_socket(cli->fd, cli->outbuf+nwritten, len - nwritten);
		if (ret <= 0 && errno == EPIPE)
			return False;
		nwritten += ret;
	}
	
	return True;
}

/****************************************************************************
See if server has cut us off by checking for EPIPE when writing.
Taken from cli_chkpath()
****************************************************************************/
static BOOL
chkpath(struct cli_state *cli, char *path, BOOL send_only)
{
	fstring path2;
	char *p;
	
	fstrcpy(path2,path);
        unix_to_dos (path2, 1);
	trim_string(path2,NULL,"\\");
	if (!*path2) *path2 = '\\';
	
	memset(cli->outbuf,'\0',smb_size);
	set_message(cli->outbuf,0,4 + strlen(path2),True);
	SCVAL(cli->outbuf,smb_com,SMBchkpth);
	SSVAL(cli->outbuf,smb_tid,cli->cnum);

	cli->rap_error = 0;
	cli->nt_error = 0;
	SSVAL(cli->outbuf,smb_pid,cli->pid);
	SSVAL(cli->outbuf,smb_uid,cli->vuid);
	SSVAL(cli->outbuf,smb_mid,cli->mid);
	if (cli->protocol > PROTOCOL_CORE) {
		SCVAL(cli->outbuf,smb_flg,0x8);
		SSVAL(cli->outbuf,smb_flg2,0x1);
	}
	
	p = smb_buf(cli->outbuf);
	*p++ = 4;
	fstrcpy(p,path2);

	if (!smb_send(cli)) {
		DEBUG(3, ("chkpath: couldnt send\n"));
		return False;
	}
	if (send_only) {
		client_receive_smb(cli->fd, cli->inbuf, cli->timeout);
		DEBUG(3, ("chkpath: send only OK\n"));
		return True;	/* just testing for EPIPE */
	}
	if (!client_receive_smb(cli->fd, cli->inbuf, cli->timeout)) {
		DEBUG(3, ("chkpath: receive error\n"));
		return False;
	}
	if ((my_errno = cli_error(cli, NULL, NULL, NULL))) {
		if (my_errno == 20 || my_errno == 13)
			return True;	/* ignore if 'not a directory' error */
		DEBUG(3, ("chkpath: cli_error: %s\n", g_strerror(my_errno)));
		return False;
	}

	return True;
}

/* #if 0 */
static int
fs (char *text)
{
	char *p, count = 0;
	p = strchr(text, '/');
	while (p) {
		count++;
		p++;
		p = strchr(p, '/');
	}
	if (count == 1)
		return strlen(text);
	return count;
}
/* #endif */

static int
smbfs_loaddir (opendir_info *smbfs_info)
{
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	int servlen = strlen(smbfs_info->conn->service);
	char *p;
/*	int retries = 0;	*/
	char *my_dirname = g_strdup(smbfs_info->dirname);
	p = my_dirname;

	DEBUG(3, ("smbfs_loaddir: dirname:%s\n", my_dirname));
	first_direntry = TRUE;

	if (current_info) {
		DEBUG(3, ("smbfs_loaddir: new:'%s', cached:'%s'\n", my_dirname, current_info->dirname));
		/* if new desired dir is longer than cached in current_info */
		if (fs(my_dirname) > fs(current_info->dirname)) {
			DEBUG(3, ("saving to previous_info\n"));
			previous_info = current_info;
		}
	}

	current_info = smbfs_info;

	if (strcmp(my_dirname, "/") == 0) {
		if (!strcmp(smbfs_info->path, URL_HEADER)) {
			DEBUG(6, ("smbfs_loaddir: browsing %s\n", IPC));
			/* browse for servers */
			if (!cli_NetServerEnum(smbfs_info->conn->cli, smbfs_info->conn->domain,
				SV_TYPE_ALL, server_browsing_helper))
					return 0;
			else
				current_server_info = smbfs_info;
			smbfs_info->server_list = TRUE;
		} else {
			/* browse for shares */
			if (cli_RNetShareEnum(smbfs_info->conn->cli, browsing_helper) < 1)
					return 0;
			else
				current_share_info = smbfs_info;
		}
		goto done;
	}
	/* do regular directory listing */
	if(strncmp(smbfs_info->conn->service, my_dirname+1, servlen) == 0) {
		/* strip share name from dir */
		my_dirname += servlen;
		*my_dirname = '/';
	}
	convert_path(&my_dirname, TRUE);

	DEBUG(6, ("smbfs_loaddir: service: %s\n", smbfs_info->conn->service));
	DEBUG(6, ("smbfs_loaddir: cli->share: %s\n", smbfs_info->conn->cli->share));
	DEBUG(6, ("smbfs_loaddir: calling cli_list with mask %s\n", my_dirname));
	/* do file listing: cli_list returns number of files */
    if (cli_list(
		smbfs_info->conn->cli, my_dirname, attribute, loaddir_helper) < 0) {
		/* cli_list returns -1 if directory empty or cannot read socket */
		my_errno = cli_error(smbfs_info->conn->cli, NULL, &err, NULL);
		return 0;
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

static void
smbfs_free_dir (dir_entry *de)
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
static void *
smbfs_readdir (void *info)
{
    char *dirent_dest;
	opendir_info  *smbfs_info = (opendir_info *) info;

	DEBUG(4, ("smbfs_readdir(%s)\n", smbfs_info->dirname));
	
    if (!smbfs_info->entries)
	    if (!smbfs_loaddir (smbfs_info))
			return NULL;

    if (smbfs_info->current == 0) {	/* reached end of dir entries */
		DEBUG(3, ("smbfs_readdir: smbfs_info->current = 0\n"));
/*	    cached_lstat_info = 0;
	    smbfs_free_dir (smbfs_info->entries);
	    smbfs_info->entries = 0;	*/
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

static int
smbfs_closedir (void *info)
{
    opendir_info *smbfs_info = (opendir_info *) info;
/*    dir_entry *p, *q;	*/

	DEBUG(3, ("smbfs_closedir(%s)\n", smbfs_info->dirname));
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

static int
smbfs_chmod (vfs *me, char *path, int mode)
{
	DEBUG(3, ("smbfs_chmod(path:%s, mode:%d)\n", path, mode));
/*	my_errno = EOPNOTSUPP;
	return -1;	*/	/* cant chmod on smb filesystem */
	return 0;		/* make mc happy */
}

static int
smbfs_chown (vfs *me, char *path, int owner, int group)
{
	DEBUG(3, ("smbfs_chown(path:%s, owner:%d, group:%d)\n", path, owner, group));
	my_errno = EOPNOTSUPP;	/* ready for your labotomy? */
	return -1;
}

static int
smbfs_utime (vfs *me, char *path, struct utimbuf *times)
{
	DEBUG(3, ("smbfs_utime(path:%s)\n", path));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int
smbfs_readlink (vfs *me, char *path, char *buf, int size)
{
	DEBUG(3, ("smbfs_readlink(path:%s, buf:%s, size:%d)\n", path, buf, size));
	my_errno = EOPNOTSUPP;
    return -1;	/* no symlinks on smb filesystem? */
}

static int
smbfs_symlink (vfs *me, char *n1, char *n2)
{
	DEBUG(3, ("smbfs_symlink(n1:%s, n2:%s)\n", n1, n2));
	my_errno = EOPNOTSUPP;
    return -1;	/* no symlinks on smb filesystem? */
}

/* Extract the hostname and username from the path */
/* path is in the form: hostname:user/remote-dir */
static char *
smbfs_get_host_and_username
(char **path, char **host, char **user, int *port, char **pass)
{
/*	char *p, *ret;	*/
	char *ret;

    ret = vfs_split_url (*path, host, user, port, pass, SMB_PORT, 0);

#if 0
	if ((p = strchr(*path, '@')))	/* user:pass@server */
		*path = ++p;				/* dont want user:pass@ in path */
	if ((p = strchr(ret, '@')))	/* user:pass@server */
		ret = ++p;				/* dont want user:pass@ in path */
#endif

	return ret;
}

/***************************************************** 
	return a connection to a SMB server
	current_bucket needs to be set before calling
*******************************************************/
struct cli_state *
do_connect (char *server, char *share)
{
	struct cli_state *c;
	struct nmb_name called, calling;
	char *server_n;
	struct in_addr ip;
	extern struct in_addr ipzero;

	DEBUG(3, ("do_connect(%s, %s)\n", server, share));
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
	if (!(c = cli_initialise(NULL))) {
		my_errno = ENOMEM;
		return NULL;
	}

	pwd_init(&(c->pwd));	/* should be moved into cli_initialise()? */
	pwd_set_cleartext(&(c->pwd), current_bucket->password);

	if ((cli_set_port(c, current_bucket->port) == 0) ||
			!cli_connect(c, server_n, &ip)) {
		my_errno = cli_error(c, NULL, &err, NULL);
		DEBUG(1, ("Connection to %s failed\n", server_n));
		return NULL;
	}

	if (!cli_session_request(c, &calling, &called)) {
		my_errno = cli_error(c, NULL, &err, NULL);
		DEBUG(1, ("session request to %s failed\n", called.name));
		cli_shutdown(c);
		if (strcmp(called.name, "*SMBSERVER")) {
			make_nmb_name(&called , "*SMBSERVER", 0x20, "");
			goto again;
		}
		return NULL;
	}

	DEBUG(3, (" session request ok\n"));

	if (!cli_negprot(c)) {
		my_errno = cli_error(c, NULL, &err, NULL);
		DEBUG(1, ("protocol negotiation failed\n"));
		cli_shutdown(c);
		return NULL;
	}

	if (!cli_session_setup(c, current_bucket->user, 
			       current_bucket->password, strlen(current_bucket->password),
			       current_bucket->password, strlen(current_bucket->password),
			       current_bucket->domain)) {
		my_errno = cli_error(c, NULL, &err, NULL);
		DEBUG(1,("session setup failed: %s\n", cli_errstr(c)));
		return NULL;
	}

	if (*c->server_domain || *c->server_os || *c->server_type)
		DEBUG(5,("Domain=[%s] OS=[%s] Server=[%s]\n",
			c->server_domain,c->server_os,c->server_type));
	
	DEBUG(3, (" session setup ok\n"));

	if (!cli_send_tconX(c, share, "?????",
			    current_bucket->password, strlen(current_bucket->password)+1)) {
		my_errno = cli_error(c, NULL, &err, NULL);
		DEBUG(1,("%s: tree connect failed: %s\n", share, cli_errstr(c)));
		cli_shutdown(c);
		return NULL;
	}

	DEBUG(3, (" tconx ok\n"));

	my_errno = 0;
	return c;
}

static int
get_master_browser(char **host)
{
	int count;
	struct in_addr *ip_list, bcast_addr, ipzero;
	/* does port = 137 for win95 master browser? */
	int fd= open_socket_in( SOCK_DGRAM, 0, 3,
                             interpret_addr(lp_socket_address()), True ); 
	if (fd == -1)
		return 0;
	set_socket_options(fd, "SO_BROADCAST");
	ip_list = iface_bcast(ipzero);
	bcast_addr = *ip_list;
	if ((ip_list = name_query(fd, "\01\02__MSBROWSE__\02", 1, True, 
		True, bcast_addr, &count, NULL))) {
		if (!count)
			return 0;
		/* just return first master browser */
		*host = g_strdup(inet_ntoa(ip_list[0]));
		return 1;
	}
	return 0;
}

static int smbfs_get_free_bucket_init = 1;

static void 
free_bucket (smbfs_connection *bucket)
{
	g_free (bucket->host);
	g_free (bucket->service);
	g_free (bucket->domain);
	g_free (bucket->user);
	wipe_password (bucket->password);
	g_free (bucket->home);
	bzero (bucket, sizeof (smbfs_connection));
}

static smbfs_connection *
smbfs_get_free_bucket ()
{
    int i;
    
    if (smbfs_get_free_bucket_init) {
		smbfs_get_free_bucket_init = 0;
		for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++)
	    	bzero (&smbfs_connections[i], sizeof (smbfs_connection));
    }
    for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++)
		if (!smbfs_connections [i].cli) return &smbfs_connections [i];

	{	/* search for most dormant connection */
		int oldest;	/* index */
		time_t oldest_time = time(NULL);
		for (i = 0; i< SMBFS_MAX_CONNECTIONS; i++) {
			if (smbfs_connections[i].last_use < oldest_time) {
				oldest_time = smbfs_connections[i].last_use;
				oldest = i;
			}
		}
		cli_shutdown(smbfs_connections[oldest].cli);
		free_bucket (&smbfs_connections[oldest]);
		return &smbfs_connections[oldest];
	}

    /* This can't happend, since we have checked for max connections before */
    vfs_die("Internal error: smbfs_get_free_bucket");
    return 0;	/* shut up, stupid gcc */
}

/* This routine keeps track of open connections */
/* Returns a connected socket to host */
static smbfs_connection *
smbfs_open_link(char *host, char *path, char *user, int *port, char *this_pass)
{
    int i;
    smbfs_connection *bucket;
	pstring service;
	struct in_addr *dest_ip = NULL;

	DEBUG(3, ("smbfs_open_link(host:%s, path:%s)\n", host, path));

	if (strcmp(host, path) == 0)	/* if host & path are same: */
		pstrcpy(service, IPC);		/* setup for browse */
	else {	/* get share name from path, path starts with server name */
		char *p;
		if ((p = strchr(path, '/')))	/* get share aka				*/
			pstrcpy(service, ++p);	/* service name from path		*/

		/* now check for trailing directory/filenames	*/
		p = strchr(service, '/');
		if (p)
			*p = 0;				/* cut off dir/files: sharename only */

		DEBUG(6, ("smbfs_open_link: service from path:%s\n", service));
	}

	if (got_user)
		user = g_strdup(username);	/* global from getenv */

    /* Is the link actually open? */
    if (smbfs_get_free_bucket_init) {
        smbfs_get_free_bucket_init = 0;
        for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++)
	    	bzero (&smbfs_connections[i], sizeof (smbfs_connection));
    } else for (i = 0; i < SMBFS_MAX_CONNECTIONS; i++) {
		if (!smbfs_connections[i].cli)
		    continue;
		if ((strcmp (host, smbfs_connections [i].host) == 0) &&
		    (strcmp (user, smbfs_connections [i].user) == 0) &&
		    (strcmp (service, smbfs_connections [i].service) == 0)) {
			int retries = 0;
			BOOL inshare = (*host != 0 && *path != 0 && strchr(path, '/'));
			/* check if this connection has died */
			while (!chkpath(smbfs_connections[i].cli, "\\", !inshare)) {
				if (!reconnect(&smbfs_connections[i], &retries))
					return 0;
			}
			DEBUG(6, ("smbfs_open_link: returning smbfs_connection[%d]\n", i));
			current_bucket = &smbfs_connections[i];
			smbfs_connections [i].last_use = time(NULL);
		    return &smbfs_connections [i];
		}
		/* connection not found, find if we have ip for new connection */
		if (strcmp (host, smbfs_connections [i].host) == 0)
			dest_ip = &smbfs_connections[i].cli->dest_ip;
    }

	/* make new connection */
    bucket = smbfs_get_free_bucket ();
	bucket->name_type = 0x20;
    bucket->home = 0;
	bucket->port = *port;
	bucket->have_ip = False;
	if (dest_ip) {
		bucket->have_ip = True;
		bucket->dest_ip = *dest_ip;
	}
	current_bucket = bucket;

	bucket->user = g_strdup(user);
	bucket->service = g_strdup (service);

	if (!strlen(host)) {	/* if blank host name, browse for servers */
		if (!get_master_browser(&host))	/* set host to ip of master browser */
			return 0;		/* couldnt find master browser? */
		bucket->host = g_strdup("");	/* blank host means master browser */
	} else
		bucket->host = g_strdup(host);

	if (!bucket_set_authinfo (bucket,
				  0,      /* domain not currently not used */
				  user, 
				  this_pass, 
				  1))
		return 0;

	/* connect to share */
    for ( ;; ) {
        if (bucket->cli = do_connect(host, service))
		break;
	if (my_errno != EPERM)
            return 0;
	message_1s (1, MSG_ERROR,
		" Authentication failed ");

        /* authentication failed, try again */
	authinfo_remove (bucket->host, bucket->service);
	if (!bucket_set_authinfo (bucket,
				  bucket->domain,
				  bucket->user, 
				  0, 
                                  0))
		return 0;

    }

    smbfs_open_connections++;
    DEBUG(3, ("smbfs_open_link:smbfs_open_connections: %d\n",
		smbfs_open_connections));
    return bucket;
}

static char *
smbfs_get_path(smbfs_connection **sc, char *path)
{
	char *user, *host, *remote_path, *pass;
	int port = SMB_PORT;

	DEBUG(3, ("smbfs_get_path(%s)\n", path));
    if (strncmp (path, URL_HEADER, HEADER_LEN))
        return NULL;
    path += HEADER_LEN;

	if (*path == '/')	/* '/' leading server name */
		path++;			/* probably came from server browsing */

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

static int
is_error (int result, int errno_num)
{
    if (!(result == -1))
		return my_errno = 0;
    else 
		my_errno = errno_num;
    return 1;
}

static void *
smbfs_opendir (vfs *me, char *dirname)
{
    opendir_info *smbfs_info;
	smbfs_connection *sc;
	char *remote_dir;

	DEBUG(3, ("smbfs_opendir(dirname:%s)\n", dirname));

    if (!(remote_dir = smbfs_get_path (&sc, dirname)))
		return NULL;

	/* FIXME: where freed? */
    smbfs_info = g_new (opendir_info, 1);
	smbfs_info->server_list = FALSE;
    smbfs_info->path = g_strdup(dirname);		/* keep original */
	smbfs_info->dirname = remote_dir;
    smbfs_info->conn = sc;
    smbfs_info->entries = 0;
    smbfs_info->current = 0;

    return smbfs_info;
}

static int
fake_server_stat(char *server_url, char *path, struct stat *buf)
{
	dir_entry *dentry;
	char *p;

	while ((p = strchr(path, '/')))
		path++;		/* advance until last '/' */

	if (!current_info->entries) {
		if (!smbfs_loaddir(current_info));	/* browse host */
			return -1;
	}
	dentry = current_info->entries;
	DEBUG(4, ("fake stat for SERVER \"%s\"\n", path));
	while (dentry) {
		if (strcmp(dentry->text, path) == 0) {
			DEBUG(4, ("fake_server_stat: %s:%d\n",
				dentry->text, dentry->my_stat.st_mode));
			memcpy(buf, &dentry->my_stat, sizeof(struct stat));
			return 0;
		}
		dentry = dentry->next;
	}
	my_errno = ENOENT;
	return -1;
}

static int
fake_share_stat(char *server_url, char *path, struct stat *buf)
{
	dir_entry *dentry;
	if (strlen(path) < strlen(server_url))
		return -1;
	path += strlen(server_url);	/*	we only want share name	*/
	path++;

	if (*path == '/')	/* '/' leading server name */
		path++;			/* probably came from server browsing */

	if (!current_share_info->entries) {
		if (!smbfs_loaddir(current_share_info));	/* browse host */
			return -1;
	}
	dentry = current_share_info->entries;
	DEBUG(3, ("fake_share_stat: %s on %s\n", path, server_url));
	while (dentry) {
		if (strcmp(dentry->text, path) == 0) {
			DEBUG(6, ("fake_share_stat: %s:%d\n",
				dentry->text, dentry->my_stat.st_mode));
			memcpy(buf, &dentry->my_stat, sizeof(struct stat));
			return 0;
		}
		dentry = dentry->next;
	}
	my_errno = ENOENT;
	return -1;
}

/* stat a single file, get_remote_stat callback  */
dir_entry *single_entry;
static void
statfile_helper(file_info *finfo, const char *mask)
{
	time_t t = finfo->mtime; /* the time is assumed to be passed as GMT */
	single_entry = g_new (dir_entry, 1);
	single_entry->text = dos_to_unix (g_strdup (finfo->name), 1);

    single_entry->next = 0;
	single_entry->my_stat.st_size = finfo->size;
	single_entry->my_stat.st_mtime = finfo->mtime;
	single_entry->my_stat.st_atime = finfo->atime;
	single_entry->my_stat.st_ctime = finfo->ctime;
	single_entry->my_stat.st_uid = finfo->uid;
	single_entry->my_stat.st_gid = finfo->gid;

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
 
	DEBUG(6, ("  %-30s%7.7s%8.0f  %s",
		CNV_LANG(finfo->name),
		attrib_string(finfo->mode),
		(double)finfo->size,
		asctime(LocalTime(&t)))); 
}

/* stat a single file */
static int
get_remote_stat(smbfs_connection *sc, char *path, struct stat *buf)
{
	uint16 attribute = aDIR | aSYSTEM | aHIDDEN;
	char *mypath = path;

	DEBUG(3, ("get_remote_stat(): mypath:%s\n", mypath));

	convert_path(&mypath, FALSE);

    if (cli_list(sc->cli, mypath, attribute, statfile_helper) < 1) {
		my_errno = ENOENT;
        g_free (mypath);
		return -1;	/* cli_list returns number of files */
	}

	memcpy(buf, &single_entry->my_stat, sizeof(struct stat));

/* dont free here, use for smbfs_fstat() */
/*	g_free(single_entry->text);
	g_free(single_entry);	*/
    g_free (mypath);
	return 0;
}

static int
search_dir_entry (dir_entry *dentry, char *text, struct stat *buf)
{
	while (dentry) {
		if (strcmp(text, dentry->text) == 0) {
			memcpy(buf, &dentry->my_stat, sizeof(struct stat));
			memcpy(&single_entry->my_stat, &dentry->my_stat,
				sizeof(struct stat));
			return 0;
		}
		dentry = dentry->next;
	}
	return -1;
}

static int
get_stat_info (smbfs_connection *sc, char *path, struct stat *buf)
{
	char *p, *mpp;
#if 0
	dir_entry *dentry = current_info->entries;
#endif
	char *mypath;
	mpp = mypath = g_strdup(path);

	mypath++;				/* cut off leading '/' */
	while ((p = strchr(mypath, '/')))
		mypath++;			/* advance until last file/dir name */
	DEBUG(3, ("get_stat_info: mypath:%s, current_info->dirname:%s\n",
		mypath, current_info->dirname));
#if 0
	if (!dentry) {
		DEBUG(1, ("No dir entries (empty dir) cached:'%s', wanted:'%s'\n", current_info->dirname, path));
		return -1;
	}
#endif
	if (!single_entry)	/* when found, this will be written too */
		single_entry = g_new (dir_entry, 1);
	if (search_dir_entry(current_info->entries, mypath, buf) == 0)
		return 0;
		/* now try to identify mypath as PARENT dir */
	{
		char *mdp;
		char *mydir;
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
			memcpy(&single_entry->my_stat, &fake_stat, sizeof(struct stat));
			g_free(mdp);
			g_free(mpp);
			DEBUG(1, ("	PARENT:found in %s\n", current_info->dirname));
			return 0;
		}
		g_free(mdp);
	}
		/* now try to identify as CURRENT dir? */
	{
		char *dnp = current_info->dirname;
		DEBUG(6, ("get_stat_info: is %s current dir? this dir is: %s\n",
			mypath, current_info->dirname));
		if (*dnp == '/')
			dnp++;
		else {
			g_free(mpp);
			return -1;
		}
		if (strcmp(mypath, dnp) == 0) {
			struct stat fake_stat;
			bzero(&fake_stat, sizeof(struct stat));
			fake_stat.st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
			memcpy(buf, &fake_stat, sizeof(struct stat));
			memcpy(&single_entry->my_stat, &fake_stat, sizeof(struct stat));
			g_free(mpp);
			DEBUG(1, ("	CURRENT:found in %s\n", current_info->dirname));
			return 0;
		}
	}
	DEBUG(3, ("'%s' not found in current_info '%s'\n", path, current_info->dirname));
	/* try to find this in the PREVIOUS listing */
	if (previous_info) {
		if (search_dir_entry(previous_info->entries, mypath, buf) == 0)
			return 0;
		DEBUG(3, ("'%s' not found in previous_info '%s'\n", path, previous_info->dirname));
	}
	/* try to find this in the SHARE listing */
	if (current_share_info && search_dir_entry(current_share_info->entries, mypath, buf) == 0)
		return 0;
	DEBUG(3, ("'%s' not found in share_info '%s'\n", path, current_share_info->dirname));
	/* try to find this in the SERVER listing */
	if (current_server_info && search_dir_entry(current_server_info->entries, mypath, buf) == 0)
		return 0;
	DEBUG(3, ("'%s' not found in server_info '%s'\n", path, current_server_info->dirname));
	/* nothing found. get stat file info from server */
	g_free(mpp);
	return get_remote_stat(sc, path, buf);
}

static int
smbfs_chdir (vfs *me, char *path)
{
	char *remote_dir;
	smbfs_connection *sc;
	
	DEBUG(3, ("smbfs_chdir(path:%s)\n", path));
    if (!(remote_dir = smbfs_get_path (&sc, path)))
		return -1;
	
    return 0;
}

static int 
loaddir(vfs *me, char *path)
{
	void *info;
	char *mypath, *p;
	mypath = g_strdup(path);
	p = mypath;
	if (*p == '/')
		p++;
	while (strchr(p, '/'))
		p++;
	if (p-mypath > 1)
		*--p = 0;
	DEBUG(6, ("loaddir(%s)\n", mypath));
	smbfs_chdir(me, mypath);
	info = smbfs_opendir (me, mypath);
	g_free(mypath);
	if (!info)
		return -1;
	smbfs_readdir(info);
	smbfs_loaddir(info);
	return 0;
}

static int
smbfs_stat (vfs *me, char *path, struct stat *buf)
{
	char *remote_dir;
	smbfs_connection *sc;
	pstring server_url;
	char *service, *p, *mypath, *pp;

	DEBUG(3, ("smbfs_stat(path:%s)\n", path));

#if 0
	if (p = strchr(path, '@'))	/* user:pass@server */
		path = ++p;				/* dont want user:pass@ in path */
#endif

	if (!current_info) {
		DEBUG(1, ("current_info = NULL: "));
		if (loaddir(me, path) < 0)
			return -1;
	}

	pstrcpy(server_url, URL_HEADER);
	pstrcat(server_url, current_bucket->host);

	/* check if stating server */
	p = mypath = g_strdup(path);
	if (strncmp(p, URL_HEADER, HEADER_LEN)) {
		DEBUG(1, ("'%s' doesnt start with '%s' (length %d)\n",
			p, URL_HEADER, HEADER_LEN));
		return -1;
	} else
		p += HEADER_LEN;
	if (*p == '/')
		p++;
	if (!strchr(p, '/')) {
		g_free(mypath);
		if (!current_info->server_list) {
			if (loaddir(me, path) < 0)
				return -1;
		}
		return fake_server_stat(server_url, path, buf);
	}
	if (strlen(p) == 0)
		DEBUG(1, ("zero length p, mypath:'%s'", mypath));
	p = strchr(p, '/');	/* advance past next '/' */
	p++;
	if (!strchr(p, '/')) {
		g_free(mypath);
		return fake_share_stat(server_url, path, buf);
	}
	g_free(mypath);

	/* stating inside share at this point */
    if (!(remote_dir = smbfs_get_path (&sc, path))) /* connects if necessary */
		return -1;

	{
		char *sp, *pp = path;
		int hostlen = strlen(current_bucket->host);
		pp += strlen(path)-hostlen;
		sp = server_url;
		sp += strlen(server_url)-hostlen;
		if (strcmp(sp, pp) == 0) {
			/* make server name appear as directory */
			DEBUG(1, ("smbfs_stat: showing server as directory\n"));
			bzero(buf, sizeof(struct stat));
			buf->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
    		return 0;
		}
	}
	/* check if current_info is in share requested */
	p = service = g_strdup(path);
	if (strcmp(p, URL_HEADER))
		p += HEADER_LEN;
	if (*p== '/')
		p++;
	pp = strchr(p, '/');
	if (pp)
		p = ++pp;	/* advance past server name */
	pp = strchr(p, '/');
	if (pp)
		*pp = 0;	/* cut off everthing after service name */
	else
		p = g_strdup(IPC);	/* browsing for services */
	pp = current_info->dirname;
	if (*pp == '/');
		pp++;
	if (strncmp(p, pp, strlen(p)) != 0) {
		DEBUG(6, ("desired '%s' is not loaded, we have '%s'\n", p, pp));
		if (loaddir(me, path) < 0)
			return -1;
		DEBUG(6, ("loaded dir: '%s'\n", current_info->dirname));
	}
	if (service)
		g_free(service);
	/* stat dirs & files under shares now */
	return get_stat_info(sc, path, buf);
}

static int
smbfs_lstat (vfs *me, char *path, struct stat *buf)
{
	DEBUG(4, ("smbfs_lstat(path:%s)\n", path));
	return smbfs_stat(me, path, buf);	/* no symlinks on smb filesystem? */
}

static int
smbfs_lseek (void *data, off_t offset, int whence)
{
	DEBUG(3, ("smbfs_lseek()\n"));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int
smbfs_mknod (vfs *me, char *path, int mode, int dev)
{
	DEBUG(3, ("smbfs_mknod(path:%s, mode:%d, dev:%d)\n", path, mode, dev));
	my_errno = EOPNOTSUPP;
    return -1;
}

static int
smbfs_mkdir (vfs *me, char *path, mode_t mode)
{
	smbfs_connection *sc;
	char *remote_file;

	DEBUG(3, ("smbfs_mkdir(path:%s, mode:%d)\n", path, mode));
	if ((remote_file = smbfs_get_path (&sc, path)) == 0)
		return -1;
 
	convert_path(&path, FALSE);

	if (!cli_mkdir(sc->cli, path)) {
		my_errno = cli_error(sc->cli, NULL, &err, NULL);
        message_3s (1, MSG_ERROR, _(" %s mkdir'ing %s "), 
			cli_errstr(sc->cli), CNV_LANG(path));
		return -1;
	} 
    return 0;
}

static int
smbfs_rmdir (vfs *me, char *path)
{
	smbfs_connection *sc;
	char *remote_file;

	DEBUG(3, ("smbfs_rmdir(path:%s)\n", path));
	if ((remote_file = smbfs_get_path (&sc, path)) == 0)
		return -1;

	convert_path(&path, FALSE);

	if (!cli_rmdir(sc->cli, path)) {
		my_errno = cli_error(sc->cli, NULL, &err, NULL);
        message_3s (1, MSG_ERROR, _(" %s rmdir'ing %s "), 
			cli_errstr(sc->cli), CNV_LANG(path));
		return -1;
	} 
    return 0;
}

static int
smbfs_link (vfs *me, char *p1, char *p2)
{
	DEBUG(3, ("smbfs_link(p1:%s, p2:%s)\n", p1, p2));
	my_errno = EOPNOTSUPP;
    return -1;
}

/* We do not free anything right now: we free resources when we run
 * out of them
 */
static vfsid
smbfs_getid (vfs *me, char *p, struct vfs_stamping **parent)
{
    *parent = NULL;
	DEBUG(3, ("smbfs_getid(p:%s)\n", p));
    
    return (vfsid) -1;
}

static int
smbfs_nothingisopen (vfsid id)
{
	DEBUG(3, ("smbfs_nothingisopen(%d)\n", (int)id));
    return 0;
}

static void
smbfs_free (vfsid id)
{
	DEBUG(3, ("smbfs_free(%d)\n", (int)id));
    /* FIXME: Should not be empty */
	authinfo_free_all ();
}

/* Gives up on a socket and reopnes the connection, the child own the socket
 * now
 */
static void
my_forget (char *path)
{
    char *host, *user, *pass, *p;
    int  port, i;

    if (strncmp (path, URL_HEADER, HEADER_LEN))
		return;

	DEBUG(3, ("my_forget(path:%s)\n", path));
    
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
	DEBUG(3, ("smbfs_setctl(path:%s, ctlop:%d)\n", path, ctlop));
    switch (ctlop) {
        case MCCTL_FORGET_ABOUT:
	    my_forget(path);
	    return 0;
    }
    return 0;
}

static smbfs_handle *
open_write (smbfs_handle *remote_handle, char *rname, int flags, int mode)
{
	if (flags & O_TRUNC)	/* if it exists truncate to zero */
		DEBUG(3, ("open_write: O_TRUNC\n"));

	remote_handle->fnum = cli_open(remote_handle->cli, rname, flags, DENY_NONE);

	if (remote_handle->fnum == -1) {
        message_3s (1, MSG_ERROR, _(" %s opening remote file %s "), 
			cli_errstr(remote_handle->cli), CNV_LANG(rname));
		DEBUG(1,("smbfs_open(rname:%s) error:%s\n",
			rname, cli_errstr(remote_handle->cli)));
		my_errno = cli_error(remote_handle->cli, NULL, &err, NULL);
    	return NULL;
	}
	
    return remote_handle;
}

static smbfs_handle *
open_read (smbfs_handle *remote_handle, char *rname, int flags, int mode)
{
	size_t		size;

	remote_handle->fnum =
		cli_open(remote_handle->cli, rname, O_RDONLY, DENY_NONE);

	if (remote_handle->fnum == -1) {
        message_3s (1, MSG_ERROR, _(" %s opening remote file %s "), 
			cli_errstr(remote_handle->cli), CNV_LANG(rname));
		DEBUG(1,("smbfs_open(rname:%s) error:%s\n",
			rname, cli_errstr(remote_handle->cli)));
		my_errno = cli_error(remote_handle->cli, NULL, &err, NULL);
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
		my_errno = cli_error(remote_handle->cli, NULL, &err, NULL);
        return NULL;
    }

    return remote_handle;
}

static void *
smbfs_open (vfs *me, char *file, int flags, int mode)
{
    char *remote_file;
	void *ret;
    smbfs_connection	*sc;
    smbfs_handle		*remote_handle;

	DEBUG(3, ("smbfs_open(file:%s, flags:%d, mode:%d)\n", file, flags, mode));

    if (!(remote_file = smbfs_get_path (&sc, file)))
		return 0;

	convert_path(&remote_file, FALSE);

    remote_handle			= g_new (smbfs_handle, 2);
    remote_handle->cli		= sc->cli;
	remote_handle->nread	= 0;

	if (flags & O_CREAT)
		ret = open_write(remote_handle, remote_file, flags, mode);
	else
		ret = open_read(remote_handle, remote_file, flags, mode);
	return ret;
}

static int
smbfs_unlink (vfs *me, char *path)
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

static int
smbfs_rename (vfs *me, char *a, char *b)
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

static int
smbfs_fstat (void *data, struct stat *buf)
{
    smbfs_handle *remote_handle = (smbfs_handle *)data;

	DEBUG(3, ("smbfs_fstat(fnum:%d)\n", remote_handle->fnum));

	/* use left over from previous get_remote_stat, if available */
	if (single_entry)
		memcpy(buf, &single_entry->my_stat, sizeof(struct stat));
	else {	/* single_entry not set up: bug */
		return -EFAULT;
		my_errno = -EFAULT;
	}
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
    NULL,

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
