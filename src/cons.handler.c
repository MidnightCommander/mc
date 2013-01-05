/*
   Client interface for General purpose Linux console save/restore server

   Copyright (C) 1994, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2011
   The Free Software Foundation, Inc. 

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file cons.handler.c
 *  \brief Source: client %interface for General purpose Linux console save/restore server
 */

#include <config.h>

#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#ifdef __FreeBSD__
#include <sys/consio.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif
#include <unistd.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/skin.h"           /* tty_set_normal_attrs */
#include "lib/tty/win.h"
#include "lib/util.h"           /* mc_build_filename() */

#include "consaver/cons.saver.h"

/*** global variables ****************************************************************************/

#ifdef __linux__
int cons_saver_pid = 1;
#endif /* __linux__ */

/*** file scope macro definitions ****************************************************************/

#if defined(__FreeBSD__)
#define FD_OUT 1
#define cursor_to(x, y) \
do \
{ \
    printf("\x1B[%d;%df", (y) + 1, (x) + 1); \
    fflush(stdout); \
} while (0)
#endif /* __linux__ */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

#ifdef __linux__
/* The cons saver can't have a pid of 1, used to prevent bunches of
 * #ifdef linux */
static int pipefd1[2] = { -1, -1 };
static int pipefd2[2] = { -1, -1 };
#elif defined(__FreeBSD__)
static struct scrshot screen_shot;
static struct vid_info screen_info;
#endif /* __linux__ */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

#ifdef __linux__
static void
show_console_contents_linux (int starty, unsigned char begin_line, unsigned char end_line)
{
    unsigned char message = 0;
    unsigned short bytes = 0;
    int i;
    ssize_t ret;

    /* Is tty console? */
    if (mc_global.tty.console_flag == '\0')
        return;
    /* Paranoid: Is the cons.saver still running? */
    if (cons_saver_pid < 1 || kill (cons_saver_pid, SIGCONT))
    {
        cons_saver_pid = 0;
        mc_global.tty.console_flag = '\0';
        return;
    }

    /* Send command to the console handler */
    message = CONSOLE_CONTENTS;
    ret = write (pipefd1[1], &message, 1);
    /* Check for outdated cons.saver */
    ret = read (pipefd2[0], &message, 1);
    if (message != CONSOLE_CONTENTS)
        return;

    /* Send the range of lines that we want */
    ret = write (pipefd1[1], &begin_line, 1);
    ret = write (pipefd1[1], &end_line, 1);
    /* Read the corresponding number of bytes */
    ret = read (pipefd2[0], &bytes, 2);

    /* Read the bytes and output them */
    for (i = 0; i < bytes; i++)
    {
        if ((i % COLS) == 0)
            tty_gotoyx (starty + (i / COLS), 0);
        ret = read (pipefd2[0], &message, 1);
        tty_print_char (message);
    }

    /* Read the value of the mc_global.tty.console_flag */
    ret = read (pipefd2[0], &message, 1);
    (void) ret;
}

/* --------------------------------------------------------------------------------------------- */

static void
handle_console_linux (console_action_t action)
{
    char *tty_name;
    char *mc_conssaver;
    int status;

    switch (action)
    {
    case CONSOLE_INIT:
        /* Close old pipe ends in case it is the 2nd time we run cons.saver */
        status = close (pipefd1[1]);
        status = close (pipefd2[0]);
        /* Create two pipes for communication */
        if (!((pipe (pipefd1) == 0) && ((pipe (pipefd2)) == 0)))
        {
            mc_global.tty.console_flag = '\0';
            break;
        }
        /* Get the console saver running */
        cons_saver_pid = fork ();
        if (cons_saver_pid < 0)
        {
            /* Cannot fork */
            /* Delete pipes */
            status = close (pipefd1[1]);
            status = close (pipefd1[0]);
            status = close (pipefd2[1]);
            status = close (pipefd2[0]);
            mc_global.tty.console_flag = '\0';
        }
        else if (cons_saver_pid > 0)
        {
            /* Parent */
            /* Close the extra pipe ends */
            status = close (pipefd1[0]);
            status = close (pipefd2[1]);
            /* Was the child successful? */
            status = read (pipefd2[0], &mc_global.tty.console_flag, 1);
            if (mc_global.tty.console_flag == '\0')
            {
                pid_t ret;
                status = close (pipefd1[1]);
                status = close (pipefd2[0]);
                ret = waitpid (cons_saver_pid, &status, 0);
                (void) ret;
            }
        }
        else
        {
            /* Child */
            /* Close the extra pipe ends */
            status = close (pipefd1[1]);
            status = close (pipefd2[0]);
            tty_name = ttyname (0);
            /* Bind the pipe 0 to the standard input */
            do
            {
                if (dup2 (pipefd1[0], 0) == -1)
                    break;
                status = close (pipefd1[0]);
                /* Bind the pipe 1 to the standard output */
                if (dup2 (pipefd2[1], 1) == -1)
                    break;

                status = close (pipefd2[1]);
                /* Bind standard error to /dev/null */
                status = open ("/dev/null", O_WRONLY);
                if (dup2 (status, 2) == -1)
                    break;
                status = close (status);
                if (tty_name)
                {
                    /* Exec the console save/restore handler */
                    mc_conssaver = mc_build_filename (SAVERDIR, "cons.saver", NULL);
                    execl (mc_conssaver, "cons.saver", tty_name, (char *) NULL);
                }
                /* Console is not a tty or execl() failed */
            }
            while (0);
            mc_global.tty.console_flag = '\0';
            status = write (1, &mc_global.tty.console_flag, 1);
            my_exit (3);
        }                       /* if (cons_saver_pid ...) */
        break;

    case CONSOLE_DONE:
    case CONSOLE_SAVE:
    case CONSOLE_RESTORE:
        /* Is tty console? */
        if (mc_global.tty.console_flag == '\0')
            return;
        /* Paranoid: Is the cons.saver still running? */
        if (cons_saver_pid < 1 || kill (cons_saver_pid, SIGCONT))
        {
            cons_saver_pid = 0;
            mc_global.tty.console_flag = '\0';
            return;
        }
        /* Send command to the console handler */
        status = write (pipefd1[1], &action, 1);
        if (action != CONSOLE_DONE)
        {
            /* Wait the console handler to do its job */
            status = read (pipefd2[0], &mc_global.tty.console_flag, 1);
        }
        if (action == CONSOLE_DONE || mc_global.tty.console_flag == '\0')
        {
            /* We are done -> Let's clean up */
            pid_t ret;
            close (pipefd1[1]);
            close (pipefd2[0]);
            ret = waitpid (cons_saver_pid, &status, 0);
            mc_global.tty.console_flag = '\0';
            (void) ret;
        }
        break;
    default:
        break;
    }
}

#elif defined(__FreeBSD__)

/* --------------------------------------------------------------------------------------------- */
/**
 * FreeBSD support copyright (C) 2003 Alexander Serkov <serkov@ukrpost.net>.
 * Support for screenmaps by Max Khon <fjoe@FreeBSD.org>
 */

static void
console_init (void)
{
    if (mc_global.tty.console_flag != '\0')
        return;

    screen_info.size = sizeof (screen_info);
    if (ioctl (FD_OUT, CONS_GETINFO, &screen_info) == -1)
        return;

    memset (&screen_shot, 0, sizeof (screen_shot));
    screen_shot.xsize = screen_info.mv_csz;
    screen_shot.ysize = screen_info.mv_rsz;
    screen_shot.buf = g_try_malloc (screen_info.mv_csz * screen_info.mv_rsz * 2);
    if (screen_shot.buf != NULL)
        mc_global.tty.console_flag = '\001';
}

/* --------------------------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------------------------- */

static void
console_restore (void)
{
    int i, last;

    if (mc_global.tty.console_flag == '\0')
        return;

    cursor_to (0, 0);

    /* restoring all content up to cursor position */
    last = screen_info.mv_row * screen_info.mv_csz + screen_info.mv_col;
    for (i = 0; i < last; ++i)
    {
        set_attr ((screen_shot.buf[i] >> 8) & 0xFF);
        putc (screen_shot.buf[i] & 0xFF, stdout);
    }

    /* restoring cursor color */
    set_attr ((screen_shot.buf[last] >> 8) & 0xFF);

    fflush (stdout);
}

/* --------------------------------------------------------------------------------------------- */

static void
console_shutdown (void)
{
    if (mc_global.tty.console_flag == '\0')
        return;

    g_free (screen_shot.buf);

    mc_global.tty.console_flag = '\0';
}

/* --------------------------------------------------------------------------------------------- */

static void
console_save (void)
{
    int i;
    scrmap_t map;
    scrmap_t revmap;

    if (mc_global.tty.console_flag == '\0')
        return;

    /* screen_info.size is already set in console_init() */
    if (ioctl (FD_OUT, CONS_GETINFO, &screen_info) == -1)
    {
        console_shutdown ();
        return;
    }

    /* handle console resize */
    if (screen_info.mv_csz != screen_shot.xsize || screen_info.mv_rsz != screen_shot.ysize)
    {
        console_shutdown ();
        console_init ();
    }

    if (ioctl (FD_OUT, CONS_SCRSHOT, &screen_shot) == -1)
    {
        console_shutdown ();
        return;
    }

    if (ioctl (FD_OUT, GIO_SCRNMAP, &map) == -1)
    {
        console_shutdown ();
        return;
    }

    for (i = 0; i < 256; i++)
    {
        char *p = memchr (map.scrmap, i, 256);
        revmap.scrmap[i] = p ? p - map.scrmap : i;
    }

    for (i = 0; i < screen_shot.xsize * screen_shot.ysize; i++)
    {
        /* *INDENT-OFF* */
        screen_shot.buf[i] = (screen_shot.buf[i] & 0xff00) 
            | (unsigned char) revmap.scrmap[screen_shot.buf[i] & 0xff];
        /* *INDENT-ON* */
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
show_console_contents_freebsd (int starty, unsigned char begin_line, unsigned char end_line)
{
    int col, line;
    char c;

    if (mc_global.tty.console_flag == '\0')
        return;

    for (line = begin_line; line <= end_line; line++)
    {
        tty_gotoyx (starty + line - begin_line, 0);
        for (col = 0; col < min (COLS, screen_info.mv_csz); col++)
        {
            c = screen_shot.buf[line * screen_info.mv_csz + col] & 0xFF;
            tty_print_char (c);
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
handle_console_freebsd (console_action_t action)
{
    switch (action)
    {
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
    default:
        break;
    }
}
#endif /* __FreeBSD__ */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
show_console_contents (int starty, unsigned char begin_line, unsigned char end_line)
{
    tty_set_normal_attrs ();

    if (look_for_rxvt_extensions ())
    {
        show_rxvt_contents (starty, begin_line, end_line);
        return;
    }
#ifdef __linux__
    show_console_contents_linux (starty, begin_line, end_line);
#elif defined (__FreeBSD__)
    show_console_contents_freebsd (starty, begin_line, end_line);
#else
    mc_global.tty.console_flag = '\0';
#endif
}

/* --------------------------------------------------------------------------------------------- */

void
handle_console (console_action_t action)
{
    (void) action;

    if (look_for_rxvt_extensions ())
        return;

#ifdef __linux__
    handle_console_linux (action);
#elif defined (__FreeBSD__)
    handle_console_freebsd (action);
#endif
}

/* --------------------------------------------------------------------------------------------- */
