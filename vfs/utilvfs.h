#ifndef __UTILVFS_H
#define __UTILVFS_H

#include "../src/global.h"

#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* message() */
#include "../src/main.h"	/* print_vfs_message */

/* Flags for vfs_split_url() */
#define URL_ALLOW_ANON 1
#define URL_NOSLASH 2

int vfs_finduid (char *name);
int vfs_findgid (char *name);

char *vfs_split_url (const char *path, char **host, char **user, int *port,
		     char **pass, int default_port, int flags);
int vfs_split_text (char *p);

int vfs_mkstemps (char **pname, const char *prefix, const char *basename);
void vfs_die (const char *msg);
char *vfs_get_password (char *msg);

int vfs_parse_ls_lga (const char *p, struct stat *s, char **filename,
		      char **linkname);
int vfs_parse_filetype (char c);
int vfs_parse_filemode (const char *p);
int vfs_parse_filedate (int idx, time_t *t);

#endif				/* !__UTILVFS_H */
