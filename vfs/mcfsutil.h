#ifndef __MCFSUTIL_H
#define __MCFSUTIL_H

/* Operations of the RPC manager */

/*
 * Please note that the RPC manager uses 4-byte integers for the
 * communication - may be a problem on systems with longer pointers.
 */

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
int socket_write_block (int sock, char *buffer, int len);
#endif				/* !__MCFSUTIL_H */
