#ifndef PIPETHROUGH_H
#define PIPETHROUGH_H

/*@-protoparamname@*/

struct pipe_inbuffer {
	/*@observer@*/ const void *data;
	size_t size;
};

struct pipe_outbuffer {
	/*@only@*/ /*@null@*/ void *data;
	size_t size;
};

extern int pipethrough(const     char                  *command,
                       const     struct pipe_inbuffer  *stdin_buf,
                       /*@out@*/ struct pipe_outbuffer *stdout_buf,
                       /*@out@*/ struct pipe_outbuffer *stderr_buf,
                       /*@out@*/ int                   *status)
/*@globals internalState, fileSystem, errno, stderr; @*/
/*@modifies internalState, fileSystem, errno, *stderr, *stdout_buf, *stderr_buf, *status; @*/;

extern void pipe_outbuffer_finalize(/*@special@*/ struct pipe_outbuffer *buf)
/*@modifies *buf; @*/
/*@releases buf->data; @*/
/*@ensures isnull buf->data; @*/;

/*@=protoparamname@*/

#endif
