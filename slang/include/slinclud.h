#ifndef _SLANG_INCLUDE_H_
#define _SLANG_INCLUDE_H_

#define SLANG_SOURCE_	1
#include "config.h"
#include "sl-feat.h"

#include <stdio.h>
#include <string.h>

#if defined(__QNX__) && defined(__WATCOMC__)
# include <unix.h>
#endif
  
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#endif				       /* _SLANG_INCLUDE_H_ */
