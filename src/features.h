#ifndef __FEATURES_H
#define __FEATURES_H

#ifdef USE_NCURSES
#define status_using_ncurses (1)
#else
#define status_using_ncurses (0)
#endif

extern void version (int verbose);

#endif /* __FEATURES_H */
