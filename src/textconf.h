#ifndef __TEXTCONF_H
#define __TEXTCONF_H

/* Features of the text mode edition */

#define COMPUTE_FORMAT_ALLOCATIONS    1
#define PORT_WIDGET_WANTS_HISTORY     1
#define PORT_NEEDS_CHANGE_SCREEN_SIZE 1
#define x_flush_events()
#define port_shutdown_extra_fds()

extern void version (int verbose);

#endif /* __TEXTCONF_H */
