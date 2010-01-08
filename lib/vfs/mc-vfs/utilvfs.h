
/**
 * \file
 * \brief Header: Utilities for VFS modules
 * \author Miguel de Icaza
 * \date 1995, 1996
 */

#ifndef MC_VFS_UTILVFS_H
#define MC_VFS_UTILVFS_H

#include <sys/stat.h>

#include "src/global.h"

/** Bit flags for vfs_split_url()
 *
 *  Modify parsing parameters according to flag meaning.
 *  @see vfs_split_url()
 */
enum VFS_URL_FLAGS {
    URL_USE_ANONYMOUS = 1, /**< if set, empty *user will contain NULL instead of current */
    URL_NOSLASH       = 2  /**< if set, 'proto://' part in url is not searched */
};

int vfs_finduid (const char *name);
int vfs_findgid (const char *name);

char *vfs_split_url (const char *path, char **host, char **user, int *port,
		     char **pass, int default_port, enum VFS_URL_FLAGS flags);
int vfs_split_text (char *p);

int vfs_mkstemps (char **pname, const char *prefix, const char *basename);
void vfs_die (const char *msg);
char *vfs_get_password (const char *msg);

char * vfs_get_local_username(void);

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
