#if defined(linux) || defined(__linux__)

/* General purpose Linux console screen save/restore server
   Copyright (C) 1994 Janne Kukonlehto <jtklehto@stekt.oulu.fi>
   Original idea from Unix Interactive Tools version 3.2b (tty.c)
   This code requires root privileges.
   You may want to make the cons.saver setuid root.
   The code should be safe even if it is setuid but who knows?
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This code does _not_ need to be setuid root. However, it needs
   read/write access to /dev/vcsa* (which is priviledged
   operation). You should create user vcsa, make cons.saver setuid
   user vcsa, and make all vcsa's owned by user vcsa.

   Seeing other peoples consoles is bad thing, but believe me, full
   root is even worse. */

#include <config.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>	/* For isdigit() */
#include <string.h>

#define LINUX_CONS_SAVER_C
#include "cons.saver.h"

#define cmd_input 0
#define cmd_output 1

/* Meaning of console_flag:
   -1 == to be detected,
   0  == not a console or Linux < 1.1.92
   1  == obsolete, not used
   2  == obsolete, not used
   3  == is a console, Linux >= 1.1.92 (color, use /dev/vcsa$num
   */
static signed char console_flag = -1;
static char *tty_name;
static int console_minor = 0;
static char *buffer = NULL;
static int buffer_size = 0;
static int columns, rows;
static int vcs_fd;


static void tty_getsize (int console_fd)
{
    struct winsize winsz;

    winsz.ws_col = winsz.ws_row = 0;
    ioctl (console_fd, TIOCGWINSZ, &winsz);
    if (winsz.ws_col && winsz.ws_row){
	columns = winsz.ws_col;
	rows    = winsz.ws_row;
    } else {
	/* Never happens (I think) */
	columns = 80;
	rows = 25;
	console_flag = 0;
    }
}

static int check_file (char *filename, int check_console)
{
    int fd;
    struct stat stat_buf;

    /* Avoiding race conditions: use of fstat makes sure that
       both 'open' and 'stat' operate on the same file */

    fd = open (filename, O_RDWR);
    if (fd == -1)
	return -1;
    
    do {
	if (fstat (fd, &stat_buf) == -1)
	    break;

	/* Must be character device */
	if (!S_ISCHR (stat_buf.st_mode)){
	    break;
	}

	if (check_console){
	    /* Major number must be 4 */
	    if ((stat_buf.st_rdev & 0xff00) != 0x0400){
		break;
	    }

	    /* Minor number must be between 1 and 63 */
	    console_minor = (int) (stat_buf.st_rdev & 0x00ff);
	    if (console_minor < 1 || console_minor > 63){
		break;
	    }

	    /* Must be owned by the user */
	    if (stat_buf.st_uid != getuid ()){
		break;
	    }
	}
	/* Everything seems to be okay */
	return fd;
    } while (0);

    close (fd);
    return -1;
}

/* Detect console. Return 0 if successful, -1 otherwise.  */
/* Because the name of the tty is supplied by the user and this
   can be a setuid program a lot of checks has to done to avoid
   creating a security hole */
static int detect_console (void)
{
    char console_name [16];
    static char vcs_name [16];
    int console_fd;

    /* Must be console */
    console_fd = check_file (tty_name, 1);
    if (console_fd == -1)
	return -1;

    /*
     * Only allow /dev/ttyMINOR and /dev/vc/MINOR where MINOR is the minor
     * device number of the console, set in check_file()
     */
    switch (tty_name[5])
    {
	case 'v':
	    snprintf (console_name, sizeof (console_name), "/dev/vc/%d",
		      console_minor);
	    if (strncmp (console_name, tty_name, sizeof (console_name)) != 0)
		return -1;
	    break;
	case 't':
	    snprintf (console_name, sizeof (console_name), "/dev/tty%d",
		      console_minor);
	    if (strncmp (console_name, tty_name, sizeof (console_name)) != 0)
		return -1;
	    break;
	default:
	    return -1;
    }

    snprintf (vcs_name, sizeof (vcs_name), "/dev/vcsa%d", console_minor);
    vcs_fd = check_file (vcs_name, 0);

    /* Try devfs name */
    if (vcs_fd == -1) {
	snprintf (vcs_name, sizeof (vcs_name), "/dev/vcc/a%d", console_minor);
	vcs_fd = check_file (vcs_name, 0);
    }

    if (vcs_fd != -1){
	console_flag = 3;
	tty_getsize (console_fd);
	close (console_fd);
	return 0;
    }

    close (console_fd);
    return -1;
}

static void save_console (void)
{
    lseek (vcs_fd, 0, 0);
    read (vcs_fd, buffer, buffer_size);
}

static void restore_console (void)
{
    lseek (vcs_fd, 0, 0);
    write (vcs_fd, buffer, buffer_size);
}

static void send_contents (void)
{
    unsigned char begin_line=0, end_line=0;
    int index;
    unsigned char message;
    unsigned short bytes;
    const int bytes_per_char = 2;

    /* Inform the invoker that we can handle this command */
    message = CONSOLE_CONTENTS;
    write (cmd_output, &message, 1);

    /* Read the range of lines wanted */
    read (cmd_input, &begin_line, 1);
    read (cmd_input, &end_line, 1);
    if (begin_line > rows)
	begin_line = rows;
    if (end_line > rows)
	end_line = rows;

    /* Tell the invoker how many bytes it will be */
    bytes = (end_line - begin_line) * columns;
    write (cmd_output, &bytes, 2);

    /* Send the contents */
    for (index = (2 + begin_line * columns) * bytes_per_char;
	 index < (2 + end_line * columns) * bytes_per_char;
	 index += bytes_per_char)
	write (cmd_output, buffer + index, 1);

    /* All done */
}

int main (int argc, char **argv)
{
    unsigned char action = 0;
    int stderr_fd;
    
    /*
     * Make sure stderr points to a valid place
     */
    close (2);
    stderr_fd = open ("/dev/tty", O_RDWR);

    /* This may well happen if program is running non-root */
    if (stderr_fd == -1)
	stderr_fd = open ("/dev/null", O_RDWR);

    if (stderr_fd == -1)
	exit (1);
    
    if (stderr_fd != 2)
	while (dup2 (stderr_fd, 2) == -1 && errno == EINTR)
	    ;
    
    if (argc != 2){
	/* Wrong number of arguments */

	console_flag = 0;
	write (cmd_output, &console_flag, 1);
	return 3;
    }

    /* Lose the control terminal */
    setsid ();
    
    /* Check that the argument is a legal console */
    tty_name = argv [1];

    if (detect_console () == -1){
	/* Not a console -> no need for privileges */
	setuid (getuid ());
	console_flag = 0;
    } else {
	/* Allocate buffer for screen image */
	buffer_size = 4 + 2 * columns * rows;
	buffer = (char*) malloc (buffer_size);
    }

    /* Inform the invoker about the result of the tests */
    write (cmd_output, &console_flag, 1);

    /* Read commands from the invoker */
    while (console_flag && read (cmd_input, &action, 1)){
	/* Handle command */
	switch (action){
	case CONSOLE_DONE:
	    console_flag = 0;
	    continue; /* Break while loop instead of switch clause */
	case CONSOLE_SAVE:
	    save_console ();
	    break;
	case CONSOLE_RESTORE:
	    restore_console ();
	    break;
	case CONSOLE_CONTENTS:
	    send_contents ();
	    break;
	} /* switch (action) */
		
	/* Inform the invoker that command is handled */
	write (cmd_output, &console_flag, 1);
    } /* while (read ...) */

    if (buffer)
	free (buffer);
    return 0;   
}

#else

 #error "The Linux console screen saver works only on Linux"

#endif /* #ifdef linux */
