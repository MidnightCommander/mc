/* Operations of the rpc manager */

/* Please note that the RPC manager does not use integers, it only uses */
/* 4-byte integers for the comunication */
enum {
    RPC_END,			/* End of RPC commands */
    RPC_INT,			/* Next argument is integer */
    RPC_STRING,			/* Next argument is a string */
    RPC_BLOCK,			/* Next argument is a len/block */
    RPC_LIMITED_STRING		/* same as STRING, but has a limit on the size it accepts */
};

int rpc_get (int sock, ...);
int rpc_send (int sock, ...);
int socket_read_block (int sock, char *dest, int len);
int socket_write_block (int sock, char *buffer, int len);
void tcp_init (void);

extern int got_sigpipe;
