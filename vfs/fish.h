/*  fish.h */

#if !defined(__FISH_H)
#define __FISH_H

/* Increased since now we may use C-r to reread the contents */
#define FISH_DIRECTORY_TIMEOUT 30 * 60

#define DO_RESOLVE_SYMLINK 1
#define DO_OPEN            2
#define DO_FREE_RESOURCE   4

#define FISH_FLAG_COMPRESSED 1
#define FISH_FLAG_RSH	     2

#define OPT_FLUSH        1
#define OPT_IGNORE_ERROR 2

#endif
