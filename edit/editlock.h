#ifndef __EDIT_LOCK_H
#define __EDIT_LOCK_H

int edit_lock_file (const char *fname);
int edit_unlock_file (const char *fname);

#endif				/* !__EDIT_LOCK_H */
