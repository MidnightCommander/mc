/* Client interface for General purpose Linux console save/restore server
   Copyright (C) 1994 Janne Kukonlehto <jtklehto@stekt.oulu.fi>
   
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

#include <config.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>

#ifdef __FreeBSD__
#include <sys/consio.h>
#include <sys/ioctl.h>
#endif				/* __FreeBSD__ */

#include "global.h"
#include "tty.h"
#include "cons.saver.h"

signed char console_flag = 0;

#ifdef __linux__

/* The cons saver can't have a pid of 1, used to prevent bunches of
 * #ifdef linux */

int cons_saver_pid = 1;
static int pipefd1[2] = { -1, -1 };
static int pipefd2[2] = { -1, -1 };

static void
show_console_contents_linux (int starty, unsigned char begin_line,
			     unsigned char end_line)
{
    unsigned char message = 0;
    unsigned short bytes = 0;
    int i;

    /* Is tty console? */
    if (!console_flag)
	return;
    /* Paranoid: Is the cons.saver still running? */
    if (cons_saver_pid < 1 || kill (cons_saver_pid, SIGCONT)) {
	cons_saver_pid = 0;
	console_flag = 0;
	return;
    }

    /* Send command to the console handler */
    message = CONSOLE_CONTENTS;
    write (pipefd1[1], &message, 1);
    /* Check for outdated cons.saver */
    read (pipefd2[0], &message, 1);
    if (message != CONSOLE_CONTENTS)
	return;

    /* Send the range of lines that we want */
    write (pipefd1[1], &begin_line, 1);
    write (pipefd1[1], &end_line, 1);
    /* Read the corresponding number of bytes */
    read (pipefd2[0], &bytes, 2);

    /* Read the bytes and output them */
    for (i = 0; i < bytes; i++) {
	if ((i % COLS) == 0)
	    move (starty + (i / COLS), 0);
	read (pipefd2[0], &message, 1);
	addch (message);
    }

    /* Read the value of the console_flag */
    read (pipefd2[0], &message, 1);
}

static void
handle_console_linux (unsigned char action)
{
    char *tty_name;
    char *mc_conssaver;
    int status;

    switch (action) {
    case CONSOLE_INIT:
	/* Close old pipe ends in case it is the 2nd time we run cons.saver */
	close (pipefd1[1]);
	close (pipefd2[0]);
	/* Create two pipes for communication */
	pipe (pipefd1);
	pipe (pipefd2);
	/* Get the console saver running */
	cons_saver_pid = fork ();
	if (cons_saver_pid < 0) {
	    /* Cannot fork */
	    /* Delete pipes */
	    close (pipefd1[1]);
	    close (pipefd1[0]);
	    close (pipefd2[1]);
	    close (pipefd2[0]);
	    console_flag = 0;
	} else if (cons_saver_pid > 0) {
	    /* Parent */
	    /* Close the extra pipe ends */
	    close (pipefd1[0]);
	    close (pipefd2[1]);
	    /* Was the child successful? */
	    read (pipefd2[0], &console_flag, 1);
	    if (!console_flag) {
		close (pipefd1[1]);
		close (pipefd2[0]);
		waitpid (cons_saver_pid, &status, 0);
	    }
	} else {
	    /* Child */
	    /* Close the extra pipe ends */
	    close (pipefd1[1]);
	    close (pipefd2[0]);
	    tty_name = ttyname (0);
	    /* Bind the pipe 0 to the standard input */
	    close (0);
	    dup (pipefd1[0]);
	    close (pipefd1[0]);
	    /* Bind the pipe 1 to the standard output */
	    close (1);
	    dup (pipefd2[1]);
	    close (pipefd2[1]);
	    /* Bind standard error to /dev/null */
	    close (2);
	    open ("/dev/null", O_WRONLY);
	    if (tty_name) {
		/* Exec the console save/restore handler */
		mc_conssaver = concat_dir_and_file (LIBDIR, "cons.saver");
		execl (mc_conssaver, "cons.saver", tty_name, (char *) NULL);
	    }
	    /* Console is not a tty or execl() failed */
	    console_flag = 0;
	    write (1, &console_flag, 1);
	    close (1);
	    close (0);
	    _exit (3);
	}			/* if (cons_saver_pid ...) */
	break;

    case CONSOLE_DONE:
    case CONSOLE_SAVE:
    case CONSOLE_RESTORE:
	/* Is tty console? */
	if (!console_flag)
	    return;
	/* Paranoid: Is the cons.saver still running? */
	if (cons_saver_pid < 1 || kill (cons_saver_pid, SIGCONT)) {
	    cons_saver_pid = 0;
	    console_flag = 0;
	    return;
	}
	/* Send command to the console handler */
	write (pipefd1[1], &action, 1);
	if (action != CONSOLE_DONE) {
	    /* Wait the console handler to do its job */
	    read (pipefd2[0], &console_flag, 1);
	}
	if (action == CONSOLE_DONE || !console_flag) {
	    /* We are done -> Let's clean up */
	    close (pipefd1[1]);
	    close (pipefd2[0]);
	    waitpid (cons_saver_pid, &status, 0);
	    console_flag = 0;
	}
	break;
    }
}

#elif defined(__FreeBSD__)

/*
 * FreeBSD support copyright (C) 2003 Alexander Serkov <serkov@ukrpost.net>.
 * Support for screenmaps by Max Khon <fjoe@FreeBSD.org>
 */

#define FD_OUT 1

static struct scrshot screen_shot;
static struct vid_info screen_info;

static void
console_init (void)
{
    if (console_flag)
	return;

    screen_info.size = sizeof (screen_info);
    if (ioctl (FD_OUT, CONS_GETINFO, &screen_info) == -1)
	return;

    memset (&screen_shot, 0, sizeof (screen_shot));
    screen_shot.xsize = screen_info.mv_csz;
    screen_shot.ysize = screen_info.mv_rsz;
    if ((screen_shot.buf =
	 g_malloc (screen_info.mv_csz * screen_info.mv_rsz * 2)) == NULL)
	return;

    console_flag = 1;
}

static void
set_attr (unsigned attr)
{
    /*
     * Convert color indices returned by SCRSHOT (red=4, green=2, blue=1)
     * to indices for ANSI sequences (red=1, green=2, blue=4).
     */
    static const int color_map[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
    int bc, tc;

    tc = attr & 0xF;
    bc = (attr >> 4) & 0xF;

    printf ("\x1B[%d;%d;3%d;4%dm", (bc & 8) ? 5 : 25, (tc & 8) ? 1 : 22,
	    color_map[tc & 7], color_map[bc & 7]);
}

#define cursor_to(x, y) do {				\
	printf("\x1B[%d;%df", (y) + 1, (x) + 1);	\
	fflush(stdout);					\
} while (0)

static void
console_restore (void)
{
    int i, last;

    if (!console_flag)
	return;

    cursor_to (0, 0);

    /* restoring all content up to cursor position */
    last = screen_info.mv_row * screen_info.mv_csz + screen_info.mv_col;
    for (i = 0; i < last; ++i) {
	set_attr ((screen_shot.buf[i] >> 8) & 0xFF);
	putc (screen_shot.buf[i] & 0xFF, stdout);
    }

    /* restoring cursor color */
    set_attr ((screen_shot.buf[last] >> 8) & 0xFF);

    fflush (stdout);
}

static void
console_shutdown (void)
{
    if (!console_flag)
	return;

    g_free (screen_shot.buf);

    console_flag = 0;
}

static void
console_save (void)
{
    int i;
    scrmap_t map;
    scrmap_t revmap;

    if (!console_flag)
	return;

    /* screen_info.size is already set in console_init() */
    if (ioctl (FD_OUT, CONS_GETINFO, &screen_info) == -1) {
	console_shutdown ();
	return;
    }

    /* handle console resize */
    if (screen_info.mv_csz != screen_shot.xsize
	|| screen_info.mv_rsz != screen_shot.ysize) {
	console_shutdown ();
	console_init ();
    }

    if (ioctl (FD_OUT, CONS_SCRSHOT, &screen_shot) == -1) {
	console_shutdown ();
	return;
    }

    if (ioctl (FD_OUT, GIO_SCRNMAP, &map) == -1) {
	console_shutdown ();
	return;
    }

    for (i = 0; i < 256; i++) {
	char *p = memchr (map.scrmap, i, 256);
	revmap.scrmap[i] = p ? p - map.scrmap : i;
    }

    for (i = 0; i < screen_shot.xsize * screen_shot.ysize; i++) {
	screen_shot.buf[i] =
	    (screen_shot.buf[i] & 0xff00) | (unsigned char) revmap.
	    scrmap[screen_shot.buf[i] & 0xff];
    }
}

static void
show_console_contents_freebsd (int starty, unsigned char begin_line,
			       unsigned char end_line)
{
    int col, line;
    char c;

    if (!console_flag)
	return;

    for (line = begin_line; line <= end_line; line++) {
	move (starty + line - begin_line, 0);
        for (col = 0; col < min (COLS, screen_info.mv_csz); col++) {
	    c = screen_shot.buf[line * screen_info.mv_csz + col] & 0xFF;
	    addch (c);
	}
    }
}

static void
handle_console_freebsd (unsigned char action)
{
    switch (action) {
    case CONSOLE_INIT:
	console_init ();
	break;

    case CONSOLE_DONE:
	console_shutdown ();
	break;

    case CONSOLE_SAVE:
	console_save ();
	break;

    case CONSOLE_RESTORE:
	console_restore ();
	break;
    }
}
#endif				/* __FreeBSD__ */

void
show_console_contents (int starty, unsigned char begin_line,
		       unsigned char end_line)
{
    standend ();

    if (look_for_rxvt_extensions ()) {
	show_rxvt_contents (starty, begin_line, end_line);
	return;
    }
#ifdef __linux__
    show_console_contents_linux (starty, begin_line, end_line);
#elif defined (__FreeBSD__)
    show_console_contents_freebsd (starty, begin_line, end_line);
#else
    console_flag = 0;
#endif
}

void
handle_console (unsigned char action)
{
    if (look_for_rxvt_extensions ())
	return;

#ifdef __linux__
    handle_console_linux (action);
#elif defined (__FreeBSD__)
    handle_console_freebsd (action);
#endif
}
