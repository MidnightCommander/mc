
/** \file
 *  \brief Header: editor file locking
 *  \author Adam Byrtek
 *  \date 2003
 *  Look at editlock.c for more details
 */

#ifndef MC_EDIT_LOCK_H
#define MC_EDIT_LOCK_H

int edit_lock_file (const char *fname);
int edit_unlock_file (const char *fname);

#endif
