/* Fully rewritten to use glib rather than S-Lang replacements */

#ifndef _JD_MACROS_H_
#define _JD_MACROS_H_

#include <glib.h>

#define SLMEMSET(x,y,z) memset(x,y,z)
#define SLMEMCPY(x,y,z) memcpy(x,y,z)
#define SLfree(x) g_free(x)
#define SLmalloc(x) g_malloc(x)
#define SLrealloc(x,y) g_realloc(x,y)
#define SLvsnprintf g_vsnprintf

#endif				/* _JD_MACROS_H_ */
