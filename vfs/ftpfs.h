#ifndef MC_VFS_FTPFS_H
#define MC_VFS_FTPFS_H

extern char *ftpfs_anonymous_passwd;
extern char *ftpfs_proxy_host;
extern int ftpfs_directory_timeout;
extern int ftpfs_always_use_proxy;

extern int ftpfs_retry_seconds;
extern int ftpfs_use_passive_connections;
extern int ftpfs_use_unix_list_options;
extern int ftpfs_first_cd_then_ls;

void ftpfs_init_passwd (void);
void init_ftpfs (void);

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

#endif
