/* Fully rewritten to use glib */

#ifndef _JD_MACROS_H_
#define _JD_MACROS_H_

#include <glib.h>

#ifndef SLMEMSET
#define SLMEMSET memset
#endif

#ifndef SLMEMCPY
#define SLMEMCPY memcpy
#endif

#define SLfree(x) g_free(x)
#define SLmalloc(x) g_malloc(x)
#define _SLvsnprintf g_vsnprintf

#endif				/* _JD_MACROS_H_ */
