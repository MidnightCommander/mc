/*  ftpfs.h */

#if !defined(__FTPFS_H)
#define __FTPFS_H

struct ftpentry
{
    char *name;
    int count;
    char *linkname;
    char *local_filename;
    int local_is_temp:1;
    int freshly_created:1;
    int tmp_reget;
    struct stat local_stat;
    char *remote_filename;
    struct stat s;
    struct stat *l_stat;
    struct ftpfs_connection *bucket;
};

struct ftpfs_dir
{
    int count;
    struct timeval timestamp;
    char *remote_path;
    struct linklist *file_list;
};

struct ftpfs_connection {
    char *host;
    char *user;
    char *current_directory;
    char *home;
    char *updir;
    char *password;
    int port;
    int sock;
    struct linklist *dcache;
    ino_t __inode_counter;
    int lock;
    int failed_on_login;	/* used to pass the failure reason to upper levels */
    int use_proxy;		/* use a proxy server */
    int result_pending;
    int use_source_route;
    int use_passive_connection;
    int isbinary;
};

#define qhost(b) (b)->host
#define quser(b) (b)->user
#define qcdir(b) (b)->current_directory
#define qport(b) (b)->port
#define qsock(b) (b)->sock
#define qlock(b) (b)->lock
#define qdcache(b) (b)->dcache
#define qhome(b) (b)->home
#define qupdir(b) (b)->updir
#define qproxy(b) (b)->use_proxy

/* Increased since now we may use C-r to reread the contents */
#define FTPFS_DIRECTORY_TIMEOUT 30 * 60

#define FTPFS_RESOLVE_SYMLINK 1
#define FTPFS_OPEN            2
#define FTPFS_FREE_RESOURCE   4

extern char *ftpfs_anonymous_passwd;
extern char *ftpfs_proxy_host;
extern ftpfs_directory_timeout;
extern ftpfs_always_use_proxy;

void ftpfs_init_passwd ();
#endif
