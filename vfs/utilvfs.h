#ifndef __UTILVFS_H
#define __UTILVFS_H

#include "../src/global.h"

#include "../src/tty.h"		/* enable/disable interrupt key */
#include "../src/wtools.h"	/* MSG_ERROR */
#include "../src/background.h"	/* mc_message */
#include "../src/main.h"	/* print_vfs_message */

/* Flags for vfs_split_url() */
#define URL_ALLOW_ANON 1
#define URL_NOSLASH 2

char *vfs_split_url (const char *path, char **host, char **user, int *port,
		     char **pass, int default_port, int flags);

int finduid (char *name);
int findgid (char *name);

#endif				/* !__UTILVFS_H */
