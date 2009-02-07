
/**
 * \file
 * \brief Header: Low-level protocol for MCFS
 *
 * \todo FIXME: This protocol uses 32-bit integers for the communication.
 * It is a problem on systems with large file support, which is now
 * default. This means that lseek is broken unless --disable-largefile
 * is used. 64-bit systems are probably broken even more.
 */

#ifndef MC_VFS_MCFSUTIL_H
#define MC_VFS_MCFSUTIL_H


enum {
    RPC_END,			/* End of RPC commands */
    RPC_INT,			/* Next argument is integer */
    RPC_STRING,			/* Next argument is a string */
    RPC_BLOCK,			/* Next argument is a len/block */
    RPC_LIMITED_STRING		/* same as STRING, but has a size limit */
};

int rpc_get (int sock, ...);
int rpc_send (int sock, ...);
int socket_read_block (int sock, char *dest, int len);
int socket_write_block (int sock, const char *buffer, int len);

#endif
