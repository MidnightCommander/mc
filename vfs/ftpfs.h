/*  ftpfs.h */

#if !defined(__FTPFS_H)
#define __FTPFS_H

struct direntry
{
    char *name;
    int count;
    char *linkname;
    char *local_filename, *remote_filename;
    int local_is_temp:1;
    int freshly_created:1;
    struct stat local_stat;
    struct stat s;
    struct stat *l_stat;
    struct connection *bucket;

    int data_sock;		/* For linear_ operations */
    int linear_state;
#define LS_NONLIN 0		/* Not using linear access at all */
#define LS_LINEAR_CLOSED 1	/* Using linear access, but not open, yet */
#define LS_LINEAR_OPEN 2	/* Using linear access, open */
};

struct dir
{
    int count;
    struct timeval timestamp;
    char *remote_path;
    struct linklist *file_list;
    int symlink_status;
};

/* valid values for dir->symlink_status */
#define FTPFS_NO_SYMLINKS          0
#define FTPFS_UNRESOLVED_SYMLINKS  1
#define FTPFS_RESOLVED_SYMLINKS    2
#define FTPFS_RESOLVING_SYMLINKS   3 

struct connection {
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
    int cwd_defered;  /* current_directory was changed but CWD command hasn't
                         been sent yet */
    int strict_rfc959_list_cmd; /* ftp server doesn't understand 
                                   "LIST -la <path>"; use "CWD <path>"/
                                   "LIST" instead */
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

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

extern char *ftpfs_anonymous_passwd;
extern char *ftpfs_proxy_host;
extern int ftpfs_directory_timeout;
extern int ftpfs_always_use_proxy;

void ftpfs_init_passwd ();
#endif
