#ifndef __DIALOG_H
#define __DIALOG_H

/* We search under the stack until we find a refresh function that covers */
/* the complete screen, and from this point we go up refreshing the */
/* individual regions */

enum {
    REFRESH_COVERS_PART,	/* If the refresh fn convers only a part */
    REFRESH_COVERS_ALL		/* If the refresh fn convers all the screen */
};

typedef void (*refresh_fn) (void *);
void push_refresh (refresh_fn new_refresh, void *parameter, int flags);
void pop_refresh (void);
void do_refresh (void);

#endif	/* __DIALOG_H */
