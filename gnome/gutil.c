/* Various utilities - Unix variants
   Copyright (C) 1998 the Free Software Foundation.

   Written 1998 by:
   Miguel de Icaza

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <signal.h>		/* my_system */
#include <limits.h>		/* INT_MAX */
#include <sys/time.h>		/* select: timeout */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#ifdef HAVE_SYS_WAIT_H
#    include <sys/wait.h>	/* my_system */
#endif
#include <errno.h>		/* my_system */
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#include <gnome.h>
#include <X11/Xlib.h>
#include <gdk/gdkprivate.h>
#include "global.h"

static struct sigaction previous_sigchld;
static int child_died_notify_handler;

/*
 * A list of childs that were executed with a temporary file
 * We remove the files when they die
 */
static GList *children;

typedef struct {
	pid_t pid;
	char  *temp_file;
} Child;

/*
 * Received when a child dies, notifies the high level routione
 * that new input is available
 */
static void
gnome_sigchld_handler (int sig)
{
	char c;
	
	if (previous_sigchld.sa_handler != SIG_IGN &&
	    previous_sigchld.sa_handler != SIG_DFL){
		(*previous_sigchld.sa_handler)(sig);
	}
	write (child_died_notify_handler, &c, sizeof (c));
}

/*
 * Invoked from the main loop when a child has died
 * deal with it
 */
static void
gnome_child_died (gpointer data, gint source, GdkInputCondition condition)
{
	GList *l;
	char c;
	
	read (source, &c, sizeof (c));
	for (l = children; l; l = l->next){
		int status;
		Child *child = l->data;
		
		if (child->pid == waitpid (child->pid, &status, WUNTRACED | WNOHANG)){
			children = g_list_remove (children, child);
				
			unlink (child->temp_file);
			g_free (child->temp_file);
			g_free (child);
		}
	}
}

int my_system_get_child_pid (int flags, const char *shell, const char *command, pid_t *pid)
{
	struct sigaction ignore, save_intr, save_quit, save_stop;
	int status = 0, i;
	static int gnome_sigchld_installed;
	
	ignore.sa_handler = SIG_IGN;
	sigemptyset (&ignore.sa_mask);
	ignore.sa_flags = 0;
    
	sigaction (SIGINT, &ignore, &save_intr);    
	sigaction (SIGQUIT, &ignore, &save_quit);

	if (!gnome_sigchld_installed){
		struct sigaction newsig;
		int monitors [2];

		pipe (monitors);
		sigemptyset (&newsig.sa_mask);
		newsig.sa_flags = 0;
		newsig.sa_handler = gnome_sigchld_handler;
		
		sigaction (SIGCHLD, &newsig, &previous_sigchld);
		gnome_sigchld_installed = 1;

		gdk_input_add (monitors [0], GDK_INPUT_READ, gnome_child_died, NULL);
	}
	
	if ((*pid = fork ()) < 0){
		return -1;
	}
	if (*pid == 0){
		const int top = max_open_files ();
		struct sigaction default_pipe;

		sigaction (SIGINT,  &save_intr, NULL);
		sigaction (SIGQUIT, &save_quit, NULL);

		/*
		 * reset sigpipe
		 */
		default_pipe.sa_handler = SIG_DFL;
		sigemptyset (&default_pipe.sa_mask);
		default_pipe.sa_flags = 0;
		
		sigaction (SIGPIPE, &default_pipe, NULL);
		
		for (i = 0; i < top; i++)
			close (i);

		/* Setup the file descriptor for the child */
		   
		/* stdin */
		open ("/dev/null", O_APPEND);

		/* stdout */
		open ("/dev/null", O_RDONLY);

		/* stderr */
		open ("/dev/null", O_RDONLY);
		
		if (!(flags & EXECUTE_WAIT))
			*pid = fork ();
		
		if (*pid == 0){
			if (flags & EXECUTE_AS_SHELL)
				execl (shell, shell, "-c", command, (char *) 0);
			else
				execlp (shell, shell, command, (char *) 0);
			/* See note below for why we use _exit () */
			_exit (127);		/* Exec error */
		} else {
			int status;

			if (flags & EXECUTE_WAIT)
				waitpid (*pid, &status, 0);

			if (flags & EXECUTE_TEMPFILE){
				Child *child;

				child = g_new (Child, 1);
				child->pid = *pid;
				child->temp_file = g_strdup (command);
			}
		}
		/* We need to use _exit instead of exit to avoid
		 * calling the atexit handlers (specifically the gdk atexit
		 * handler
		 */
		_exit (0);
	}
	waitpid (*pid, &status, 0);
	sigaction (SIGINT,  &save_intr, NULL);
	sigaction (SIGQUIT, &save_quit, NULL);
	sigaction (SIGTSTP, &save_stop, NULL);

	return WEXITSTATUS(status);
}

int my_system (int flags, const char *shell, const char *command)
{
	pid_t pid;
	
	return my_system_get_child_pid (flags, shell, command, &pid);
}

int
exec_direct (char *path, char *argv [])
{
	struct sigaction ignore, save_intr, save_quit, save_stop;
	pid_t pid;
	int status = 0, i;

	ignore.sa_handler = SIG_IGN;
	sigemptyset (&ignore.sa_mask);
	ignore.sa_flags = 0;
    
	sigaction (SIGINT, &ignore, &save_intr);    
	sigaction (SIGQUIT, &ignore, &save_quit);

	if ((pid = fork ()) < 0){
		fprintf (stderr, "\n\nfork () = -1\n");
		return -1;
	}
	if (pid == 0){
		sigaction (SIGINT,  &save_intr, NULL);
		sigaction (SIGQUIT, &save_quit, NULL);

		for (i = 3; i < 4096; i++)
			close (i);

		execvp (path, argv); 
		_exit (127);		/* Exec error */
	}
	sigaction (SIGINT,  &save_intr, NULL);
	sigaction (SIGQUIT, &save_quit, NULL);
	sigaction (SIGTSTP, &save_stop, NULL);

#ifdef SCO_FLAVOR 
	waitpid(-1, NULL, WNOHANG);
#endif /* SCO_FLAVOR */

	return WEXITSTATUS(status);
}

void
port_shutdown_extra_fds (void)
{
	const int top = max_open_files ();
	int f;
	
	for (f = 0; f < top; f++)
		close (f);
}
