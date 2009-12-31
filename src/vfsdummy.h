
/** \file vfsdummy.h
 *  \brief Header: replacement for vfs.h if VFS support is disabled
 */

#ifndef MC_VFSDUMMY_H
#define MC_VFSDYMMY_H

#include "global.h"		/* glib.h*/
#include "util.h"

static inline int
return_zero (void)
{
    return 0;
}

#endif				/* MC_VFSDUMMY_H */
