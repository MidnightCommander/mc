#ifndef MC_EDIT_ETAGS_H
#define MC_EDIT_ETAGS_H 1

#include <sys/types.h>		/* size_t */
#include "src/global.h"	/* include <glib.h> */

#define MAX_WIDTH_DEF_DIALOG 60	/* max width def dialog */
#define MAX_DEFINITIONS 60	/* count found entries show */
#define SHORT_DEF_LEN   30
#define LONG_DEF_LEN    40
#define LINE_DEF_LEN    16

typedef struct etags_hash_struct {
   size_t filename_len;
   char *fullpath;
   char *filename;
   char *short_define;
   long line;
} etags_hash_t;

int etags_set_definition_hash (const char *tagfile, const char *start_path,
				const char *match_func, etags_hash_t *def_hash);

#endif				/* MC_EDIT_ETAGS_H */
