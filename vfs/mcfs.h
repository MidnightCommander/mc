#define mcserver_port 9876

/* This number was registered for program "mcfs" with rpc@Sun.COM */
#define RPC_PROGNUM 300516
#define RPC_PROGVER 2

/* this constants must be kept in sync with mcserv.c commands */
/* They are the messages sent on the link connection */
enum {
    MC_OPEN,
    MC_CLOSE,
    MC_READ,
    MC_WRITE,
    MC_OPENDIR,
    MC_READDIR,
    MC_CLOSEDIR,
    MC_STAT,
    MC_LSTAT,
    MC_FSTAT,
    MC_CHMOD,
    MC_CHOWN,
    MC_READLINK,
    MC_UNLINK,
    MC_RENAME,
    MC_CHDIR,
    MC_LSEEK,
    MC_RMDIR,
    MC_SYMLINK,
    MC_MKNOD,
    MC_MKDIR,
    MC_LINK,
    MC_GETHOME,
    MC_GETUPDIR,

    /* Control commands */
    MC_LOGIN,
    MC_QUIT,
    
    MC_UTIME,           /* it has to go here for compatibility with old
			   servers/clients. sigh ... */

    MC_INVALID_PASS = 0x1000,
    MC_NEED_PASSWORD,
    MC_LOGINOK,
    MC_VERSION_OK,
    MC_VERSION_MISMATCH,
    MC_PASS

};

extern char *mcfs_current_dir;
