
/** \file
 *  \brief Header: file locking
 *  \author Adam Byrtek
 *  \date 2003
 *  Look at lock.c for more details
 */

#ifndef MC_LOCK_H
#define MC_LOCK_H

int lock_file (const char *fname);
int unlock_file (const char *fname);

#endif /* MC_LOCK_H */
