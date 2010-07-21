
/**
 * \file
 * \brief Header: Virtual File System: Network utilities
 */

#ifndef MC_VFS_NETUTIL_H
#define MC_VFS_NETUTIL_H

#include <signal.h>

extern volatile sig_atomic_t got_sigpipe;
void tcp_init (void);

#endif /* MC_VFS_NETUTIL_H */
