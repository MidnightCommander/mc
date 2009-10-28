
/**
 * \file
 * \brief Header: Utilities for VFS modules
 * \author Miguel de Icaza
 * \date 1995, 1996
 */

#ifndef MC_VFS_UTILVFS_H
#define MC_VFS_UTILVFS_H

#include <sys/stat.h>

#include <glib.h>

/* Flags for vfs_split_url() */
#define URL_ALLOW_ANON 1
#define URL_NOSLASH 2

int vfs_finduid (const char *name);
int vfs_findgid (const char *name);

char *vfs_split_url (const char *path, char **host, char **user, int *port,
		     char **pass, int default_port, int flags);
int vfs_split_text (char *p);

int vfs_mkstemps (char **pname, const char *prefix, const char *basename);
void vfs_die (const char *msg);
char *vfs_get_password (const char *msg);

gboolean vfs_parse_filetype (const char *s, size_t *ret_skipped,
			     mode_t *ret_type);
gboolean vfs_parse_fileperms (const char *s, size_t *ret_skipped,
			      mode_t *ret_perms);
gboolean vfs_parse_filemode (const char *s, size_t *ret_skipped,
			     mode_t *ret_mode);
gboolean vfs_parse_raw_filemode (const char *s, size_t *ret_skipped,
			     mode_t *ret_mode);

int vfs_parse_ls_lga (const char *p, struct stat *s, char **filename,
		      char **linkname);
int vfs_parse_filedate (int idx, time_t *t);

#endif
