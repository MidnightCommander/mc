/*  fish.h */

#if !defined(__FISH_H)
#define __FISH_H

struct direntry
{
    char *name;
    int count;
    char *linkname;
    char *local_filename;
    int local_is_temp:1;
    int freshly_created:1;
    struct stat local_stat;
    char *remote_filename;
    struct stat s;
    struct stat *l_stat;
    struct connection *bucket;

    int got, total;	/* Bytes transfered / bytes need to be transfered */
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
};

struct connection {
    char *host;
    char *user;
    char *current_directory;
    char *home;
    char *updir;
    char *password;
    int flags;
    int sockr, sockw;
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
};

#define qhost(b) (b)->host
#define quser(b) (b)->user
#define qcdir(b) (b)->current_directory
#define qflags(b) (b)->flags
#define qsockr(b) (b)->sockr
#define qsockw(b) (b)->sockw
#define qlock(b) (b)->lock
#define qdcache(b) (b)->dcache
#define qhome(b) (b)->home
#define qupdir(b) (b)->updir
#define qproxy(b) (b)->use_proxy

/* Increased since now we may use C-r to reread the contents */
#define FISH_DIRECTORY_TIMEOUT 30 * 60

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

#define FISH_FLAG_COMPRESSED 1
#define FISH_FLAG_RSH	     2

#endif
