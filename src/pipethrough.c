/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Roland Illig <roland.illig@gmx.de>, 2004.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* On Solaris, FD_SET invokes memset(3) */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include <unistd.h>

#include "pipethrough.h"

#define PIPE_RD 0
#define PIPE_WR 1

/* Internal data types *************************************************/

struct writer_buffer {
	/*@temp@*/ const void *data;
	size_t size;
	size_t pos;
	int fd;
	int lasterror;
};

struct reader_buffer {
	/*@null@*/ /*@only@*/ void *data;
	size_t size;
	size_t maxsize;
	int fd;
	int lasterror;
};

struct select_request {
	int n;
	fd_set readfds;
	fd_set writefds;
};

/* Helper functions ****************************************************/

static int closeref(int *fd)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *fd; @*/
{
	int result = close(*fd);
	*fd = -1;
	return result;
}

static void propagate(int *dest, int src)
	/*@modifies *dest; @*/
{
	if (*dest == 0) {
		*dest = src;
	}
}

/* reader_buffer -- implementation *************************************/

static void reader_buffer_init(/*@out@*/ struct reader_buffer *buf, int fd)
	/*@modifies *buf; @*/
{
	buf->data = NULL;
	buf->size = 0;
	buf->maxsize = 0;
	buf->fd = fd;
	buf->lasterror = 0;
}

/* SPlint cannot describe realloc(3) correctly. */
/*@-usereleased@*/ /*@-compdef@*/ /*@-branchstate@*/ /*@-compmempass@*/
static ssize_t reader_buffer_read(struct reader_buffer *buf)
	/*@globals internalState, errno; @*/
	/*@modifies internalState, errno, *buf; @*/
{
	ssize_t res;

	if (buf->size == buf->maxsize) {
		size_t newsize = buf->maxsize + 4096;
		/*@only@*/ void *newdata;
		if (buf->data == NULL) {
			newdata = malloc(newsize);
		} else {
			newdata = realloc(buf->data, newsize);
		}
		if (newdata == NULL) {
			return (ssize_t) -1;
		}
		buf->data = newdata;
		newdata = NULL;
		buf->maxsize = newsize;
	}
	res = read(buf->fd, &(((char *) buf->data)[buf->size]),
	           buf->maxsize - buf->size);
	if (res == (ssize_t) -1) {
		return res;
	}
	buf->size += (ssize_t) res;
	return res;
}
/*@=usereleased@*/ /*@=compdef@*/ /*@=branchstate@*/ /*@=compmempass@*/

static void reader_buffer_handle(struct reader_buffer *buf)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *buf; @*/
{
	ssize_t rd = reader_buffer_read(buf);
	if (rd == (ssize_t) -1) {
		propagate(&buf->lasterror, errno);
	}
	if (rd <= 0) {
		if (closeref(&(buf->fd)) == -1) {
			propagate(&buf->lasterror, errno);
		}
	}
}

/*@-mustfreeonly@*/
static void reader_buffer_finalize(/*@special@*/ struct reader_buffer *buf)
	/*@modifies *buf; @*/
	/*@releases buf->data; @*/
{
	if (buf->data != NULL) {
		free(buf->data);
		buf->data = NULL;
	}
	buf->size = 0;
	buf->maxsize = 0;
	buf->fd = -1;
}
/*@=mustfreeonly@*/

/* writer_buffer -- implementation *************************************/

static void writer_buffer_init(/*@out@*/ struct writer_buffer *buf, int fd,
                               const void *data, size_t size)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *buf; @*/
{
	buf->fd = fd;
	buf->data = data;
	buf->size = size;
	buf->pos = 0;
	buf->lasterror = 0;
	if (size == 0) {
		if (closeref(&(buf->fd)) == -1) {
			propagate(&buf->lasterror, errno);
		}
	}
}

static void writer_buffer_handle(struct writer_buffer *buf)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *buf; @*/
{
	typedef void (*my_sighandler_fn) (int);
	my_sighandler_fn old_sigpipe = signal(SIGPIPE, SIG_IGN);
	ssize_t wr = write(buf->fd, &(((const char *) buf->data)[buf->pos]),
	                   buf->size - buf->pos);
	if (wr == (ssize_t) -1) {
		propagate(&buf->lasterror, errno);
	} else {
		buf->pos += wr;
	}
	if (wr == (ssize_t) -1 || buf->pos == buf->size) {
		if (closeref(&(buf->fd)) == -1) {
			propagate(&buf->lasterror, errno);
		}
	}
	(void) signal(SIGPIPE, old_sigpipe);
}

static void writer_buffer_finalize(/*@special@*/ struct writer_buffer *buf)
	/*@modifies *buf; @*/
{
	buf->data = NULL;
	buf->size = 0;
	buf->pos = 0;
	buf->fd = -1;
}

/* select_request -- implementation ************************************/

static void select_request_init(/*@out@*/ struct select_request *sr)
	/*@modifies *sr; @*/
{
	FD_ZERO(&(sr->readfds));
	FD_ZERO(&(sr->writefds));
	sr->n = 0;
}

static void select_request_add_reader(struct select_request *sr,
                                      const struct reader_buffer *buf)
	/*@modifies *sr; @*/
{
	if (buf->fd != -1) {
		FD_SET(buf->fd, &(sr->readfds));
		if (buf->fd + 1 > sr->n) {
			sr->n = buf->fd + 1;
		}
	}
}

static void select_request_add_writer(struct select_request *sr,
                                      const struct writer_buffer *buf)
	/*@modifies *sr; @*/
{
	if (buf->fd != -1) {
		FD_SET(buf->fd, &(sr->writefds));
		if (buf->fd + 1 > sr->n) {
			sr->n = buf->fd + 1;
		}
	}
}

static void select_request_handle_writer(const struct select_request *sr,
                                         struct writer_buffer *buf)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *buf; @*/
{
	if (buf->fd != -1 && FD_ISSET(buf->fd, &(sr->writefds))) {
		writer_buffer_handle(buf);
	}
}

static void select_request_handle_reader(const struct select_request *sr,
                                         struct reader_buffer *buf)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno, *buf; @*/
{
	if (buf->fd != -1 && FD_ISSET(buf->fd, &(sr->readfds))) {
		reader_buffer_handle(buf);
	}
}

/* pipethrough -- helper functions *************************************/

static void pipethrough_child(const char *command,
                              const int *stdin_pipe,
			      const int *stdout_pipe,
			      const int *stderr_pipe)
	/*@globals internalState, fileSystem, errno, stderr; @*/
	/*@modifies internalState, fileSystem, errno, *stderr; @*/
{
	const char *shell = getenv("SHELL");
	if (shell == NULL) {
		shell = "/bin/sh";
	}

	if (close(STDIN_FILENO) == -1
	    || dup2(stdin_pipe[PIPE_RD], STDIN_FILENO) == -1
	    || close(stdin_pipe[PIPE_RD]) == -1
	    || close(stdin_pipe[PIPE_WR]) == -1
	    || close(STDOUT_FILENO) == -1
	    || dup2(stdout_pipe[PIPE_WR], STDOUT_FILENO) == -1
	    || close(stdout_pipe[PIPE_RD]) == -1
	    || close(stdout_pipe[PIPE_WR]) == -1
	    || close(STDERR_FILENO) == -1
	    || dup2(stderr_pipe[PIPE_WR], STDERR_FILENO) == -1
	    || close(stderr_pipe[PIPE_RD]) == -1
	    || close(stderr_pipe[PIPE_WR]) == -1
	    || execl(shell, shell, "-c", command, NULL) == -1) {
	        perror("pipethrough_child");
	        (void) fflush(stderr);
		_exit(EXIT_FAILURE);
	}
}

static int pipethrough_parent(int stdin_fd,
                              int stdout_fd,
                              int stderr_fd,
			      const     struct pipe_inbuffer  *stdin_buf,
			      /*@out@*/ struct pipe_outbuffer *stdout_buf,
			      /*@out@*/ struct pipe_outbuffer *stderr_buf)
	/*@globals internalState, fileSystem, errno; @*/
	/*@modifies internalState, fileSystem, errno,
		*stdout_buf, *stderr_buf; @*/
{
	struct writer_buffer stdin_wbuf;
	struct reader_buffer stdout_rbuf;
	struct reader_buffer stderr_rbuf;
	struct select_request sr;
	int select_res;
	int firsterror;

	firsterror = 0;
	writer_buffer_init(&stdin_wbuf, stdin_fd, stdin_buf->data,
	                   stdin_buf->size);
	propagate(&firsterror, stdin_wbuf.lasterror);
	reader_buffer_init(&stdout_rbuf, stdout_fd);
	propagate(&firsterror, stdout_rbuf.lasterror);
	reader_buffer_init(&stderr_rbuf, stderr_fd);
	propagate(&firsterror, stdout_rbuf.lasterror);

	while (stdin_wbuf.fd != -1 || stdout_rbuf.fd != -1
	       || stderr_rbuf.fd != -1) {
		select_request_init(&sr);

	      retry_select:
		select_request_add_writer(&sr, &stdin_wbuf);
		select_request_add_reader(&sr, &stdout_rbuf);
		select_request_add_reader(&sr, &stderr_rbuf);
		select_res = select(sr.n, &(sr.readfds), &(sr.writefds),
		                    NULL, NULL);
		if (select_res == -1) {
			if (errno == EINTR) {
				goto retry_select;
			} else {
				propagate(&firsterror, errno);
				goto cleanup_select;
			}
		}
		select_request_handle_writer(&sr, &stdin_wbuf);
		propagate(&firsterror, stdin_wbuf.lasterror);
		select_request_handle_reader(&sr, &stdout_rbuf);
		propagate(&firsterror, stdout_rbuf.lasterror);
		select_request_handle_reader(&sr, &stderr_rbuf);
		propagate(&firsterror, stdout_rbuf.lasterror);
	}
	stdout_buf->data = stdout_rbuf.data;
	stdout_rbuf.data = NULL;
	stdout_buf->size = stdout_rbuf.size;
	stderr_buf->data = stderr_rbuf.data;
	stderr_rbuf.data = NULL;
	stderr_buf->size = stderr_rbuf.size;
	return 0;

cleanup_select:
	writer_buffer_finalize(&stdin_wbuf);
	reader_buffer_finalize(&stdout_rbuf);
	reader_buffer_finalize(&stderr_rbuf);
	errno = firsterror;
	return -1;
}

/* pipethrough -- implementation ***************************************/

extern int pipethrough(const     char                   *command,
                       const     struct pipe_inbuffer  *stdin_buf,
                       /*@out@*/ struct pipe_outbuffer *stdout_buf,
                       /*@out@*/ struct pipe_outbuffer *stderr_buf,
                       /*@out@*/ int                   *status)
	/*@globals internalState, fileSystem, errno, stderr; @*/
	/*@modifies internalState, fileSystem, errno, *stderr,
	            *stdout_buf, *stderr_buf, *status; @*/
{
	int retval = -1;
	pid_t pid;
	int stdin_pipe[2] = { -1, -1 };
	int stdout_pipe[2] = { -1, -1 };
	int stderr_pipe[2] = { -1, -1 };
	int firsterror = 0;

	if (pipe(stdin_pipe) == -1
	    || pipe(stdout_pipe) == -1
	    || pipe(stderr_pipe) == -1) {
	    	propagate(&firsterror, errno);
		goto cleanup;
	}

	if ((pid = fork()) == (pid_t) -1) {
	    	propagate(&firsterror, errno);
		goto cleanup;
	}

	if (pid == 0) {
		pipethrough_child(command, stdin_pipe, stdout_pipe,
		                  stderr_pipe);
		/*@notreached@*/
	}

	if (closeref(&stdin_pipe[PIPE_RD]) == -1
	    || closeref(&stdout_pipe[PIPE_WR]) == -1
	    || closeref(&stderr_pipe[PIPE_WR]) == -1) {
		propagate(&firsterror, errno);
		goto cleanup;
	}

	if (pipethrough_parent(stdin_pipe[PIPE_WR], stdout_pipe[PIPE_RD],
	                       stderr_pipe[PIPE_RD], stdin_buf,
	                       stdout_buf, stderr_buf) == -1) {
		propagate(&firsterror, errno);
	} else {
		retval = 0;
	}

	/*@-branchstate@*/
	if (waitpid(pid, status, 0) == (pid_t) -1) {
		propagate(&firsterror, errno);
		pipe_outbuffer_finalize(stdout_buf);
		pipe_outbuffer_finalize(stderr_buf);
		retval = -1;
	}
	/*@=branchstate@*/

cleanup:
	(void) close(stderr_pipe[PIPE_RD]);
	(void) close(stderr_pipe[PIPE_WR]);
	(void) close(stdout_pipe[PIPE_RD]);
	(void) close(stdout_pipe[PIPE_WR]);
	(void) close(stdin_pipe[PIPE_RD]);
	(void) close(stdin_pipe[PIPE_WR]);
	if (retval == -1) {
		errno = firsterror;
	}
	return retval;
}

/* pipe_outbuffer -- implementation ************************************/

/*@-mustfreeonly@*/ /*@-nullstate@*/
extern void pipe_outbuffer_finalize(struct pipe_outbuffer *buf)
	/*@modifies *buf; @*/
	/*@releases buf->data; @*/
	/*@ensures isnull buf->data; @*/
{
	if (buf->data != NULL) {
		free(buf->data);
		buf->data = NULL;
	}
	buf->size = 0;
}
/*@=mustfreeonly@*/ /*@=nullstate@*/
