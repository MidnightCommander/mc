#ifndef __DIRHIST_H
#define __DIRHIST_H

#define DIRECTORY_HISTORY_LOAD_COUNT 20

void directory_history_load (void);
void directory_history_save (void);
void directory_history_free (void);
void directory_history_init_iterator (void);
char *directory_history_get_next (void);
void directory_history_add (char *directory);

#endif
