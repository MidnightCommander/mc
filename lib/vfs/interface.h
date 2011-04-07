#ifndef MC__VFS_INTERFACE_H
#define MC__VFS_INTERFACE_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

ssize_t mc_read (int handle, void *buffer, size_t count);
ssize_t mc_write (int handle, const void *buffer, size_t count);
int mc_utime (const char *path, struct utimbuf *times);
int mc_readlink (const char *path, char *buf, size_t bufsiz);
int mc_close (int handle);
off_t mc_lseek (int fd, off_t offset, int whence);
DIR *mc_opendir (const char *dirname);
struct dirent *mc_readdir (DIR * dirp);
int mc_closedir (DIR * dir);
int mc_stat (const char *path, struct stat *buf);
int mc_mknod (const char *path, mode_t mode, dev_t dev);
int mc_link (const char *name1, const char *name2);
int mc_mkdir (const char *path, mode_t mode);
int mc_rmdir (const char *path);
int mc_fstat (int fd, struct stat *buf);
int mc_lstat (const char *path, struct stat *buf);
int mc_symlink (const char *name1, const char *name2);
int mc_rename (const char *original, const char *target);
int mc_chmod (const char *path, mode_t mode);
int mc_chown (const char *path, uid_t owner, gid_t group);
int mc_chdir (const char *path);
int mc_unlink (const char *path);
int mc_ctl (int fd, int ctlop, void *arg);
int mc_setctl (const char *path, int ctlop, void *arg);
int mc_open (const char *filename, int flags, ...);
char *mc_get_current_wd (char *buffer, size_t bufsize);
char *mc_getlocalcopy (const char *pathname);
int mc_ungetlocalcopy (const char *pathname, const char *local, int has_changed);

/*** inline functions ****************************************************************************/

#endif
