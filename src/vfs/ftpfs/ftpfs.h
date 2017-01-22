/**
 * \file
 * \brief Header: Virtual File System: FTP file system
 */

#ifndef MC__VFS_FTPFS_H
#define MC__VFS_FTPFS_H

/*** typedefs(not structures) and defined constants **********************************************/

#define FTP_INET         1
#define FTP_INET6        2

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

extern gboolean ftpfs_use_netrc;
extern char *ftpfs_anonymous_passwd;
extern char *ftpfs_proxy_host;
extern int ftpfs_directory_timeout;
extern gboolean ftpfs_always_use_proxy;
extern gboolean ftpfs_ignore_chattr_errors;

extern int ftpfs_retry_seconds;
extern gboolean ftpfs_use_passive_connections;
extern gboolean ftpfs_use_passive_connections_over_proxy;
extern gboolean ftpfs_use_unix_list_options;
extern gboolean ftpfs_first_cd_then_ls;

/*** declarations of public functions ************************************************************/

void ftpfs_init_passwd (void);
void init_ftpfs (void);

/*** inline functions ****************************************************************************/
#endif
