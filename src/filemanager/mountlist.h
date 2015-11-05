/*
   Declarations for list of mounted filesystems
 */

/** \file mountlist.h
 *  \brief Header: list of mounted filesystems
 */

#ifndef MC__MOUNTLIST_H
#define MC__MOUNTLIST_H

#include <stdint.h>             /* uintmax_t */

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* Filesystem status */
struct my_statfs
{
    int type;
    char *typename;
    const char *mpoint;
    const char *mroot;
    const char *device;
    uintmax_t avail;            /* in kB */
    uintmax_t total;            /* in kB */
    uintmax_t nfree;
    uintmax_t nodes;
};

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void init_my_statfs (void);
void my_statfs (struct my_statfs *myfs_stats, const char *path);
void free_my_statfs (void);

/*** inline functions ****************************************************************************/

#endif /* MC__MOUNTLIST_H */
