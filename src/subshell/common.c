/*
   Concurrent shell support for the Midnight Commander

   Copyright (C) 1994-2017
   Free Software Foundation, Inc.

   Written by:
   Alexander Kriegisch <Alexander@Kriegisch.name>
   Aliaksey Kandratsenka <alk@tut.by>
   Andreas Mohr <and@gmx.li>
   Andrew Borodin <aborodin@vmail.ru>
   Andrew Borodin <borodin@borodin.zarya>
   Andrew V. Samoilov <sav@bcs.zp.ua>
   Chris Owen <chris@candu.co.uk>
   Claes Nästén <me@pekdon.net>
   Egmont Koblinger <egmont@gmail.com>
   Enrico Weigelt, metux IT service <weigelt@metux.de>
   Igor Urazov <z0rc3r@gmail.com>
   Ilia Maslakov <il.smind@gmail.com>
   Leonard den Ottolander <leonard@den.ottolander.nl>
   Miguel de Icaza <miguel@novell.com>
   Mikhail S. Pobolovets <styx.mp@gmail.com>
   Norbert Warmuth <nwarmuth@privat.circular.de>
   Patrick Winnertz <winnie@debian.org>
   Pavel Machek <pavel@suse.cz>
   Pavel Roskin <proski@gnu.org>
   Pavel Tsekov <ptsekov@gmx.net>
   Roland Illig <roland.illig@gmx.de>
   Sergei Trofimovich <slyfox@inbox.ru>
   Slava Zanko <slavazanko@gmail.com>, 2013,2015.
   Timur Bakeyev <mc@bat.ru>
   Vit Rosin <vit_r@list.ru>

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

/** \file subshell.c
 *  \brief Source: concurrent shell support
 */

#include <config.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <termios.h>

#ifdef HAVE_STROPTS_H
#include <stropts.h>            /* For I_PUSH */
#endif /* HAVE_STROPTS_H */

#include "lib/global.h"

#include "lib/unixcompat.h"
#include "lib/tty/tty.h"        /* LINES */
#include "lib/tty/key.h"        /* XCTRL */
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "subshell.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/* State of the subshell:
 * INACTIVE: the default state; awaiting a command
 * ACTIVE: remain in the shell until the user hits 'subshell_switch_key'
 * RUNNING_COMMAND: return to MC when the current command finishes */
enum subshell_state_enum subshell_state;

/* Holds the latest prompt captured from the subshell */
GString *subshell_prompt = NULL;

/* Subshell: if set, then the prompt was not saved on CONSOLE_SAVE */
/* We need to paint it after CONSOLE_RESTORE, see: load_prompt */
gboolean update_subshell_prompt = FALSE;

/*** file scope macro definitions ****************************************************************/

#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif

#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

/* Initial length of the buffer for the subshell's prompt */
#define INITIAL_PROMPT_SIZE 10

/* Used by the child process to indicate failure to start the subshell */
#define FORK_FAILURE 69         /* Arbitrary */

/* Length of the buffer for all I/O with the subshell */
#define PTY_BUFFER_SIZE BUF_SMALL       /* Arbitrary; but keep it >= 80 */

/*** file scope type declarations ****************************************************************/

/* For pipes */
enum
{
    READ = 0,
    WRITE = 1
};

/*** file scope variables ************************************************************************/

/* tcsh closes all non-standard file descriptors, so we have to use a pipe */
static char tcsh_fifo[128];

static int subshell_pty_slave = -1;

/* The key for switching back to MC from the subshell */
/* *INDENT-OFF* */
static const char subshell_switch_key = XCTRL ('o') & 255;
/* *INDENT-ON* */

/* For reading/writing on the subshell's pty */
static char pty_buffer[PTY_BUFFER_SIZE] = "\0";

/* To pass CWD info from the subshell to MC */
static int subshell_pipe[2];

/* The subshell's process ID */
static pid_t subshell_pid = 1;

/* One extra char for final '\n' */
static char subshell_cwd[MC_MAXPATHLEN + 1];

/* Flag to indicate whether the subshell is ready for next command */
static int subshell_ready;

/* The following two flags can be changed by the SIGCHLD handler. This is */
/* OK, because the 'int' type is updated atomically on all known machines */
static volatile int subshell_alive, subshell_stopped;

/* We store the terminal's initial mode here so that we can configure
   the pty similarly, and also so we can restore the real terminal to
   sanity if we have to exit abruptly */
static struct termios shell_mode;

/* This is a transparent mode for the terminal where MC is running on */
/* It is used when the shell is active, so that the control signals */
/* are delivered to the shell pty */
static struct termios raw_mode;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */
/**
 *  Write all data, even if the write() call is interrupted.
 */

static ssize_t
write_all (int fd, const void *buf, size_t count)
{
    ssize_t written = 0;

    while (count > 0)
    {
        ssize_t ret;

        ret = write (fd, (const unsigned char *) buf + written, count);
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                if (mc_global.tty.winch_flag != 0)
                    tty_change_screen_size ();

                continue;
            }

            return written > 0 ? written : ret;
        }
        count -= ret;
        written += ret;
    }
    return written;
}

/* --------------------------------------------------------------------------------------------- */
/**
 *  Prepare child process to running the shell and run it.
 *
 *  Modifies the global variables (in the child process only):
 *      shell_mode
 *
 *  Returns: never.
 */

static void
init_subshell_child (const char *pty_name)
{
    char *init_file = NULL;
    char *putenv_str = NULL;
    pid_t mc_sid;

    (void) pty_name;
    setsid ();                  /* Get a fresh terminal session */

    /* Make sure that it has become our controlling terminal */

    /* Redundant on Linux and probably most systems, but just in case: */

#ifdef TIOCSCTTY
    ioctl (subshell_pty_slave, TIOCSCTTY, 0);
#endif

    /* Configure its terminal modes and window size */

    /* Set up the pty with the same termios flags as our own tty */
    if (tcsetattr (subshell_pty_slave, TCSANOW, &shell_mode))
    {
        fprintf (stderr, "Cannot set pty terminal modes: %s\r\n", unix_error_string (errno));
        my_exit (FORK_FAILURE);
    }

    /* Set the pty's size (80x25 by default on Linux) according to the */
    /* size of the real terminal as calculated by ncurses, if possible */
    tty_resize (subshell_pty_slave);

    /* Set up the subshell's environment and init file name */

    /* It simplifies things to change to our home directory here, */
    /* and the user's startup file may do a 'cd' command anyway   */
    {
        int ret;

        ret = chdir (mc_config_get_home_dir ());        /* FIXME? What about when we re-run the subshell? */
        (void) ret;
    }

    /* Set MC_SID to prevent running one mc from another */
    mc_sid = getsid (0);
    if (mc_sid != -1)
    {
        char sid_str[BUF_SMALL];

        g_snprintf (sid_str, sizeof (sid_str), "MC_SID=%ld", (long) mc_sid);
        putenv (g_strdup (sid_str));
    }

    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
        /* Do we have a custom init file ~/.local/share/mc/bashrc? */
        init_file = mc_config_get_full_path ("bashrc");

        /* Otherwise use ~/.bashrc */
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (".bashrc");
        }

        /* Make MC's special commands not show up in bash's history and also suppress
         * consecutive identical commands*/
        putenv ((char *) "HISTCONTROL=ignoreboth");

        /* Allow alternative readline settings for MC */
        {
            char *input_file;

            input_file = mc_config_get_full_path ("inputrc");
            if (exist_file (input_file))
            {
                putenv_str = g_strconcat ("INPUTRC=", input_file, (char *) NULL);
                putenv (putenv_str);
            }
            g_free (input_file);
        }

        break;

    case SHELL_ASH_BUSYBOX:
    case SHELL_DASH:
        /* Do we have a custom init file ~/.local/share/mc/ashrc? */
        init_file = mc_config_get_full_path ("ashrc");

        /* Otherwise use ~/.profile */
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (".profile");
        }

        /* Put init file to ENV variable used by ash */
        putenv_str = g_strconcat ("ENV=", init_file, (char *) NULL);
        putenv (putenv_str);
        /* Do not use "g_free (putenv_str)" here, otherwise ENV will be undefined! */

        break;

        /* TODO: Find a way to pass initfile to TCSH, ZSH and FISH */
    case SHELL_TCSH:
    case SHELL_ZSH:
    case SHELL_FISH:
        break;

    default:
        fprintf (stderr, __FILE__ ": unimplemented subshell type %u\r\n", mc_global.shell->type);
        my_exit (FORK_FAILURE);
    }

    /* Attach all our standard file descriptors to the pty */

    /* This is done just before the fork, because stderr must still      */
    /* be connected to the real tty during the above error messages; */
    /* otherwise the user will never see them.                   */

    dup2 (subshell_pty_slave, STDIN_FILENO);
    dup2 (subshell_pty_slave, STDOUT_FILENO);
    dup2 (subshell_pty_slave, STDERR_FILENO);

    close (subshell_pipe[READ]);
    close (subshell_pty_slave); /* These may be FD_CLOEXEC, but just in case... */
    /* Close master side of pty.  This is important; apart from */
    /* freeing up the descriptor for use in the subshell, it also       */
    /* means that when MC exits, the subshell will get a SIGHUP and     */
    /* exit too, because there will be no more descriptors pointing     */
    /* at the master side of the pty and so it will disappear.  */
    close (mc_global.tty.subshell_pty);

    /* Execute the subshell at last */

    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
        execl (mc_global.shell->path, "bash", "-rcfile", init_file, (char *) NULL);
        break;

    case SHELL_ZSH:
        /* Use -g to exclude cmds beginning with space from history
         * and -Z to use the line editor on non-interactive term */
        execl (mc_global.shell->path, "zsh", "-Z", "-g", (char *) NULL);

        break;

    case SHELL_ASH_BUSYBOX:
    case SHELL_DASH:
    case SHELL_TCSH:
    case SHELL_FISH:
        execl (mc_global.shell->path, mc_global.shell->path, (char *) NULL);
        break;

    default:
        break;
    }

    /* If we get this far, everything failed miserably */
    g_free (init_file);
    g_free (putenv_str);
    my_exit (FORK_FAILURE);
}


/* --------------------------------------------------------------------------------------------- */
/**
 * Check MC_SID to prevent running one mc from another.
 * Return:
 * 0 if no parent mc in our session was found,
 * 1 if parent mc was found and the user wants to continue,
 * 2 if parent mc was found and the user wants to quit mc.
 */

static int
check_sid (void)
{
    pid_t my_sid, old_sid;
    const char *sid_str;
    int r;

    sid_str = getenv ("MC_SID");
    if (sid_str == NULL)
        return 0;

    old_sid = (pid_t) strtol (sid_str, NULL, 0);
    if (old_sid == 0)
        return 0;

    my_sid = getsid (0);
    if (my_sid == -1)
        return 0;

    /* The parent mc is in a different session, it's OK */
    if (old_sid != my_sid)
        return 0;

    r = query_dialog (_("Warning"),
                      _("GNU Midnight Commander is already\n"
                        "running on this terminal.\n"
                        "Subshell support will be disabled."), D_ERROR, 2, _("&OK"), _("&Quit"));

    return (r != 0) ? 2 : 1;
}

/* --------------------------------------------------------------------------------------------- */

static void
init_raw_mode (void)
{
    static gboolean initialized = FALSE;

    /* MC calls tty_reset_shell_mode() in pre_exec() to set the real tty to its */
    /* original settings.  However, here we need to make this tty very raw,     */
    /* so that all keyboard signals, XON/XOFF, etc. will get through to the     */
    /* pty.  So, instead of changing the code for execute(), pre_exec(),        */
    /* etc, we just set up the modes we need here, before each command.         */

    if (!initialized)           /* First time: initialise 'raw_mode' */
    {
        tcgetattr (STDOUT_FILENO, &raw_mode);
        raw_mode.c_lflag &= ~ICANON;    /* Disable line-editing chars, etc.   */
        raw_mode.c_lflag &= ~ISIG;      /* Disable intr, quit & suspend chars */
        raw_mode.c_lflag &= ~ECHO;      /* Disable input echoing              */
        raw_mode.c_iflag &= ~IXON;      /* Pass ^S/^Q to subshell undisturbed */
        raw_mode.c_iflag &= ~ICRNL;     /* Don't translate CRs into LFs       */
        raw_mode.c_oflag &= ~OPOST;     /* Don't postprocess output           */
        raw_mode.c_cc[VTIME] = 0;       /* IE: wait forever, and return as    */
        raw_mode.c_cc[VMIN] = 1;        /* soon as a character is available   */
        initialized = TRUE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Wait until the subshell dies or stops.  If it stops, make it resume.
 * Possibly modifies the globals 'subshell_alive' and 'subshell_stopped'
 */

static void
synchronize (void)
{
    sigset_t sigchld_mask, old_mask;

    sigemptyset (&sigchld_mask);
    sigaddset (&sigchld_mask, SIGCHLD);
    sigprocmask (SIG_BLOCK, &sigchld_mask, &old_mask);

    /*
     * SIGCHLD should not be blocked, but we unblock it just in case.
     * This is known to be useful for cygwin 1.3.12 and older.
     */
    sigdelset (&old_mask, SIGCHLD);

    /* Wait until the subshell has stopped */
    while (subshell_alive && !subshell_stopped)
        sigsuspend (&old_mask);

    if (subshell_state != ACTIVE)
    {
        /* Discard all remaining data from stdin to the subshell */
        tcflush (subshell_pty_slave, TCIFLUSH);
    }

    subshell_stopped = FALSE;
    kill (subshell_pid, SIGCONT);

    sigprocmask (SIG_SETMASK, &old_mask, NULL);
    /* We can't do any better without modifying the shell(s) */
}

/* --------------------------------------------------------------------------------------------- */
/** Feed the subshell our keyboard input until it says it's finished */

static gboolean
feed_subshell (int how, gboolean fail_on_error)
{
    fd_set read_set;            /* For 'select' */
    int bytes;                  /* For the return value from 'read' */
    int i;                      /* Loop counter */

    struct timeval wtime;       /* Maximum time we wait for the subshell */
    struct timeval *wptr;

    /* we wait up to 10 seconds if fail_on_error, forever otherwise */
    wtime.tv_sec = 10;
    wtime.tv_usec = 0;
    wptr = fail_on_error ? &wtime : NULL;

    while (TRUE)
    {
        int maxfdp;

        if (!subshell_alive)
            return FALSE;

        /* Prepare the file-descriptor set and call 'select' */

        FD_ZERO (&read_set);
        FD_SET (mc_global.tty.subshell_pty, &read_set);
        FD_SET (subshell_pipe[READ], &read_set);
        maxfdp = MAX (mc_global.tty.subshell_pty, subshell_pipe[READ]);
        if (how == VISIBLY)
        {
            FD_SET (STDIN_FILENO, &read_set);
            maxfdp = MAX (maxfdp, STDIN_FILENO);
        }

        if (select (maxfdp + 1, &read_set, NULL, NULL, wptr) == -1)
        {
            /* Despite using SA_RESTART, we still have to check for this */
            if (errno == EINTR)
            {
                if (mc_global.tty.winch_flag != 0)
                    tty_change_screen_size ();

                continue;       /* try all over again */
            }
            tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
            fprintf (stderr, "select (FD_SETSIZE, &read_set...): %s\r\n",
                     unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        if (FD_ISSET (mc_global.tty.subshell_pty, &read_set))
            /* Read from the subshell, write to stdout */

            /* This loop improves performance by reducing context switches
               by a factor of 20 or so... unfortunately, it also hangs MC
               randomly, because of an apparent Linux bug.  Investigate. */
            /* for (i=0; i<5; ++i)  * FIXME -- experimental */
        {
            bytes = read (mc_global.tty.subshell_pty, pty_buffer, sizeof (pty_buffer));

            /* The subshell has died */
            if (bytes == -1 && errno == EIO && !subshell_alive)
                return FALSE;

            if (bytes <= 0)
            {
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr, "read (subshell_pty...): %s\r\n", unix_error_string (errno));
                exit (EXIT_FAILURE);
            }

            if (how == VISIBLY)
                write_all (STDOUT_FILENO, pty_buffer, bytes);
        }

        else if (FD_ISSET (subshell_pipe[READ], &read_set))
            /* Read the subshell's CWD and capture its prompt */
        {
            bytes = read (subshell_pipe[READ], subshell_cwd, sizeof (subshell_cwd));
            if (bytes <= 0)
            {
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr, "read (subshell_pipe[READ]...): %s\r\n",
                         unix_error_string (errno));
                exit (EXIT_FAILURE);
            }

            subshell_cwd[bytes - 1] = 0;        /* Squash the final '\n' */

            synchronize ();

            subshell_ready = TRUE;
            if (subshell_state == RUNNING_COMMAND)
            {
                subshell_state = INACTIVE;
                return TRUE;
            }
        }

        else if (FD_ISSET (STDIN_FILENO, &read_set))
            /* Read from stdin, write to the subshell */
        {
            bytes = read (STDIN_FILENO, pty_buffer, sizeof (pty_buffer));
            if (bytes <= 0)
            {
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr,
                         "read (STDIN_FILENO, pty_buffer...): %s\r\n", unix_error_string (errno));
                exit (EXIT_FAILURE);
            }

            for (i = 0; i < bytes; ++i)
                if (pty_buffer[i] == subshell_switch_key)
                {
                    write_all (mc_global.tty.subshell_pty, pty_buffer, i);
                    if (subshell_ready)
                        subshell_state = INACTIVE;
                    return TRUE;
                }

            write_all (mc_global.tty.subshell_pty, pty_buffer, bytes);

            if (pty_buffer[bytes - 1] == '\n' || pty_buffer[bytes - 1] == '\r')
                subshell_ready = FALSE;
        }
        else
            return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* pty opening functions */

#ifdef HAVE_GRANTPT

/* System V version of pty_open_master */

static int
pty_open_master (char *pty_name)
{
    char *slave_name;
    int pty_master;

#ifdef HAVE_POSIX_OPENPT
    pty_master = posix_openpt (O_RDWR);
#elif defined HAVE_GETPT
    /* getpt () is a GNU extension (glibc 2.1.x) */
    pty_master = getpt ();
#elif defined IS_AIX
    strcpy (pty_name, "/dev/ptc");
    pty_master = open (pty_name, O_RDWR);
#else
    strcpy (pty_name, "/dev/ptmx");
    pty_master = open (pty_name, O_RDWR);
#endif

    if (pty_master == -1)
        return -1;

    if (grantpt (pty_master) == -1      /* Grant access to slave */
        || unlockpt (pty_master) == -1  /* Clear slave's lock flag */
        || !(slave_name = ptsname (pty_master)))        /* Get slave's name */
    {
        close (pty_master);
        return -1;
    }
    strcpy (pty_name, slave_name);
    return pty_master;
}

/* --------------------------------------------------------------------------------------------- */
/** System V version of pty_open_slave */

static int
pty_open_slave (const char *pty_name)
{
    int pty_slave;

    pty_slave = open (pty_name, O_RDWR);
    if (pty_slave == -1)
    {
        fprintf (stderr, "open (%s, O_RDWR): %s\r\n", pty_name, unix_error_string (errno));
        return -1;
    }
#if !defined(__osf__) && !defined(__linux__)
#if defined (I_FIND) && defined (I_PUSH)
    if (ioctl (pty_slave, I_FIND, "ptem") == 0)
        if (ioctl (pty_slave, I_PUSH, "ptem") == -1)
        {
            fprintf (stderr, "ioctl (%d, I_PUSH, \"ptem\") failed: %s\r\n",
                     pty_slave, unix_error_string (errno));
            close (pty_slave);
            return -1;
        }

    if (ioctl (pty_slave, I_FIND, "ldterm") == 0)
        if (ioctl (pty_slave, I_PUSH, "ldterm") == -1)
        {
            fprintf (stderr,
                     "ioctl (%d, I_PUSH, \"ldterm\") failed: %s\r\n",
                     pty_slave, unix_error_string (errno));
            close (pty_slave);
            return -1;
        }
#if !defined(sgi) && !defined(__sgi)
    if (ioctl (pty_slave, I_FIND, "ttcompat") == 0)
        if (ioctl (pty_slave, I_PUSH, "ttcompat") == -1)
        {
            fprintf (stderr,
                     "ioctl (%d, I_PUSH, \"ttcompat\") failed: %s\r\n",
                     pty_slave, unix_error_string (errno));
            close (pty_slave);
            return -1;
        }
#endif /* sgi || __sgi */
#endif /* I_FIND && I_PUSH */
#endif /* __osf__ || __linux__ */

    fcntl (pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}

#else /* !HAVE_GRANTPT */

/* --------------------------------------------------------------------------------------------- */
/** BSD version of pty_open_master */
static int
pty_open_master (char *pty_name)
{
    int pty_master;
    const char *ptr1, *ptr2;

    strcpy (pty_name, "/dev/ptyXX");
    for (ptr1 = "pqrstuvwxyzPQRST"; *ptr1; ++ptr1)
    {
        pty_name[8] = *ptr1;
        for (ptr2 = "0123456789abcdef"; *ptr2 != '\0'; ++ptr2)
        {
            pty_name[9] = *ptr2;

            /* Try to open master */
            pty_master = open (pty_name, O_RDWR);
            if (pty_master == -1)
            {
                if (errno == ENOENT)    /* Different from EIO */
                    return -1;  /* Out of pty devices */
                continue;       /* Try next pty device */
            }
            pty_name[5] = 't';  /* Change "pty" to "tty" */
            if (access (pty_name, 6) != 0)
            {
                close (pty_master);
                pty_name[5] = 'p';
                continue;
            }
            return pty_master;
        }
    }
    return -1;                  /* Ran out of pty devices */
}

/* --------------------------------------------------------------------------------------------- */
/** BSD version of pty_open_slave */

static int
pty_open_slave (const char *pty_name)
{
    int pty_slave;
    struct group *group_info;

    group_info = getgrnam ("tty");
    if (group_info != NULL)
    {
        /* The following two calls will only succeed if we are root */
        /* [Commented out while permissions problem is investigated] */
        /* chown (pty_name, getuid (), group_info->gr_gid);  FIXME */
        /* chmod (pty_name, S_IRUSR | S_IWUSR | S_IWGRP);   FIXME */
    }
    pty_slave = open (pty_name, O_RDWR);
    if (pty_slave == -1)
        fprintf (stderr, "open (pty_name, O_RDWR): %s\r\n", pty_name);
    fcntl (pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}
#endif /* !HAVE_GRANTPT */


/* --------------------------------------------------------------------------------------------- */
/**
 * Set up `precmd' or equivalent for reading the subshell's CWD.
 *
 * Attention! Never forget that these are *one-liners* even though the concatenated
 * substrings contain line breaks and indentation for better understanding of the
 * shell code. It is vital that each one-liner ends with a line feed character ("\n" ).
 *
 * @return initialized pre-command string
 */

static void
init_subshell_precmd (char *precmd, size_t buff_size)
{
    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
        g_snprintf (precmd, buff_size,
                    " PROMPT_COMMAND=${PROMPT_COMMAND:+$PROMPT_COMMAND\n}'pwd>&%d;kill -STOP $$'\n"
                    "PS1='\\u@\\h:\\w\\$ '\n", subshell_pipe[WRITE]);
        break;

    case SHELL_ASH_BUSYBOX:
        /* BusyBox ash needs a somewhat complicated precmd emulation via PS1, and it is vital
         * that BB be built with active CONFIG_ASH_EXPAND_PRMT, but this is the default anyway.
         *
         * A: This leads to a stopped subshell (=frozen mc) if user calls "ash" command
         *    "PS1='$(pwd>&%d; kill -STOP $$)\\u@\\h:\\w\\$ '\n",
         *
         * B: This leads to "sh: precmd: not found" in sub-subshell if user calls "ash" command
         *    "precmd() { pwd>&%d; kill -STOP $$; }; "
         *    "PS1='$(precmd)\\u@\\h:\\w\\$ '\n",
         *
         * C: This works if user calls "ash" command because in sub-subshell
         *    PRECMD is unfedined, thus evaluated to empty string - no damage done.
         *    Attention: BusyBox must be built with FEATURE_EDITING_FANCY_PROMPT to
         *    permit \u, \w, \h, \$ escape sequences. Unfortunately this cannot be guaranteed,
         *    especially on embedded systems where people try to save space, so let's use
         *    the dash version below. It should work on virtually all systems.
         *    "precmd() { pwd>&%d; kill -STOP $$; }; "
         *    "PRECMD=precmd; "
         *    "PS1='$(eval $PRECMD)\\u@\\h:\\w\\$ '\n",
         */
    case SHELL_DASH:
        /* Debian ash needs a precmd emulation via PS1, similar to BusyBox ash,
         * but does not support escape sequences for user, host and cwd in prompt.
         * Attention! Make sure that the buffer for precmd is big enough.
         *
         * We want to have a fancy dynamic prompt with user@host:cwd just like in the BusyBox
         * examples above, but because replacing the home directory part of the path by "~" is
         * complicated, it bloats the precmd to a size > BUF_SMALL (128).
         *
         * The following example is a little less fancy (home directory not replaced)
         * and shows the basic workings of our prompt for easier understanding:
         *
         * "precmd() { "
         *     "echo \"$USER@$(hostname -s):$PWD\"; "
         *     "pwd>&%d; "
         *     "kill -STOP $$; "
         * "}; "
         * "PRECMD=precmd; "
         * "PS1='$($PRECMD)$ '\n",
         */
        g_snprintf (precmd, buff_size,
                    "precmd() { "
                    "if [ ! \"${PWD##$HOME}\" ]; then "
                    "MC_PWD=\"~\"; "
                    "else "
                    "[ \"${PWD##$HOME/}\" = \"$PWD\" ] && MC_PWD=\"$PWD\" || MC_PWD=\"~/${PWD##$HOME/}\"; "
                    "fi; "
                    "echo \"$USER@$(hostname -s):$MC_PWD\"; "
                    "pwd>&%d; "
                    "kill -STOP $$; "
                    "}; " "PRECMD=precmd; " "PS1='$($PRECMD)$ '\n", subshell_pipe[WRITE]);
        break;

    case SHELL_ZSH:
        g_snprintf (precmd, buff_size,
                    " _mc_precmd(){ pwd>&%d;kill -STOP $$ }; precmd_functions+=(_mc_precmd)\n"
                    "PS1='%%n@%%m:%%~%%# '\n", subshell_pipe[WRITE]);
        break;

    case SHELL_TCSH:
        g_snprintf (precmd, buff_size,
                    "set echo_style=both; "
                    "set prompt='%%n@%%m:%%~%%# '; "
                    "alias precmd 'echo $cwd:q >>%s; kill -STOP $$'\n", tcsh_fifo);
        break;

    case SHELL_FISH:
        /* We also want a fancy user@host:cwd prompt here, but fish makes it very easy to also
         * use colours, which is what we will do. But first here is a simpler, uncoloured version:
         * "function fish_prompt; "
         *     "echo (whoami)@(hostname -s):(pwd)\\$\\ ; "
         *     "echo \"$PWD\">&%d; "
         *     "kill -STOP %%self; "
         * "end\n",
         *
         * TODO: fish prompt is shown when panel is hidden (Ctrl-O), but not when it is visible.
         * Find out how to fix this.
         */
        g_snprintf (precmd, buff_size,
                    " if not functions -q fish_prompt_mc;"
                    "functions -c fish_prompt fish_prompt_mc; end;"
                    "function fish_prompt;"
                    "echo (whoami)@(hostname -s):(set_color $fish_color_cwd)(pwd)(set_color normal)\\$\\ ; "
                    "echo \"$PWD\">&%d; fish_prompt_mc; kill -STOP %%self; end\n",
                    subshell_pipe[WRITE]);
        break;

    default:
        break;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Carefully quote directory name to allow entering any directory safely,
 * no matter what weird characters it may contain in its name.
 * NOTE: Treat directory name an untrusted data, don't allow it to cause
 * executing any commands in the shell.  Escape all control characters.
 * Use following technique:
 *
 * printf(1) with format string containing a single conversion specifier,
 * "b", and an argument which contains a copy of the string passed to 
 * subshell_name_quote() with all characters, except digits and letters,
 * replaced by the backslash-escape sequence \0nnn, where "nnn" is the
 * numeric value of the character converted to octal number.
 * 
 *   cd "`printf "%b" 'ABC\0nnnDEF\0nnnXYZ'`"
 *
 */

static GString *
subshell_name_quote (const char *s)
{
    GString *ret;
    const char *su, *n;
    const char *quote_cmd_start, *quote_cmd_end;

    if (mc_global.shell->type == SHELL_FISH)
    {
        quote_cmd_start = "(printf \"%b\" '";
        quote_cmd_end = "')";
    }
    /* TODO: When BusyBox printf is fixed, get rid of this "else if", see
       http://lists.busybox.net/pipermail/busybox/2012-March/077460.html */
    /* else if (subshell_type == ASH_BUSYBOX)
       {
       quote_cmd_start = "\"`echo -en '";
       quote_cmd_end = "'`\"";
       } */
    else
    {
        quote_cmd_start = "\"`printf \"%b\" '";
        quote_cmd_end = "'`\"";
    }

    ret = g_string_sized_new (64);

    /* Prevent interpreting leading '-' as a switch for 'cd' */
    if (s[0] == '-')
        g_string_append (ret, "./");

    /* Copy the beginning of the command to the buffer */
    g_string_append (ret, quote_cmd_start);

    /*
     * Print every character except digits and letters as a backslash-escape
     * sequence of the form \0nnn, where "nnn" is the numeric value of the
     * character converted to octal number.
     */
    for (su = s; su[0] != '\0'; su = n)
    {
        n = str_cget_next_char_safe (su);

        if (str_isalnum (su))
            g_string_append_len (ret, su, n - su);
        else
        {
            int c;

            for (c = 0; c < n - su; c++)
                g_string_append_printf (ret, "\\0%03o", (unsigned char) su[c]);
        }
    }

    g_string_append (ret, quote_cmd_end);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/**
 *  Fork the subshell, and set up many, many things.
 *
 *  Possibly modifies the global variables:
 *      subshell_type, subshell_alive, subshell_stopped, subshell_pid
 *      mc_global.tty.use_subshell - Is set to FALSE if we can't run the subshell
 *      quit - Can be set to SUBSHELL_EXIT by the SIGCHLD handler
 */

void
init_subshell (void)
{
    /* This must be remembered across calls to init_subshell() */
    static char pty_name[BUF_SMALL];
    /* Must be considerably longer than BUF_SMALL (128) to support fancy shell prompts */
    char precmd[BUF_MEDIUM];

    switch (check_sid ())
    {
    case 1:
        mc_global.tty.use_subshell = FALSE;
        return;
    case 2:
        mc_global.tty.use_subshell = FALSE;
        mc_global.midnight_shutdown = TRUE;
        return;
    default:
        break;
    }

    /* Take the current (hopefully pristine) tty mode and make */
    /* a raw mode based on it now, before we do anything else with it */
    init_raw_mode ();

    if (mc_global.tty.subshell_pty == 0)
    {                           /* First time through */
        if (mc_global.shell->type == SHELL_NONE)
            return;

        /* Open a pty for talking to the subshell */

        /* FIXME: We may need to open a fresh pty each time on SVR4 */

        mc_global.tty.subshell_pty = pty_open_master (pty_name);
        if (mc_global.tty.subshell_pty == -1)
        {
            fprintf (stderr, "Cannot open master side of pty: %s\r\n", unix_error_string (errno));
            mc_global.tty.use_subshell = FALSE;
            return;
        }
        subshell_pty_slave = pty_open_slave (pty_name);
        if (subshell_pty_slave == -1)
        {
            fprintf (stderr, "Cannot open slave side of pty %s: %s\r\n",
                     pty_name, unix_error_string (errno));
            mc_global.tty.use_subshell = FALSE;
            return;
        }

        /* Create a pipe for receiving the subshell's CWD */

        if (mc_global.shell->type == SHELL_TCSH)
        {
            g_snprintf (tcsh_fifo, sizeof (tcsh_fifo), "%s/mc.pipe.%d",
                        mc_tmpdir (), (int) getpid ());
            if (mkfifo (tcsh_fifo, 0600) == -1)
            {
                fprintf (stderr, "mkfifo(%s) failed: %s\r\n", tcsh_fifo, unix_error_string (errno));
                mc_global.tty.use_subshell = FALSE;
                return;
            }

            /* Opening the FIFO as O_RDONLY or O_WRONLY causes deadlock */

            if ((subshell_pipe[READ] = open (tcsh_fifo, O_RDWR)) == -1
                || (subshell_pipe[WRITE] = open (tcsh_fifo, O_RDWR)) == -1)
            {
                fprintf (stderr, _("Cannot open named pipe %s\n"), tcsh_fifo);
                perror (__FILE__ ": open");
                mc_global.tty.use_subshell = FALSE;
                return;
            }
        }
        else if (pipe (subshell_pipe))  /* subshell_type is BASH, ASH_BUSYBOX, DASH or ZSH */
        {
            perror (__FILE__ ": couldn't create pipe");
            mc_global.tty.use_subshell = FALSE;
            return;
        }
    }

    /* Fork the subshell */

    subshell_alive = TRUE;
    subshell_stopped = FALSE;
    subshell_pid = fork ();

    if (subshell_pid == -1)
    {
        fprintf (stderr, "Cannot spawn the subshell process: %s\r\n", unix_error_string (errno));
        /* We exit here because, if the process table is full, the */
        /* other method of running user commands won't work either */
        exit (EXIT_FAILURE);
    }

    if (subshell_pid == 0)
    {
        /* We are in the child process */
        init_subshell_child (pty_name);
    }

    init_subshell_precmd (precmd, BUF_MEDIUM);

    write_all (mc_global.tty.subshell_pty, precmd, strlen (precmd));

    /* Wait until the subshell has started up and processed the command */

    subshell_state = RUNNING_COMMAND;
    tty_enable_interrupt_key ();
    if (!feed_subshell (QUIETLY, TRUE))
    {
        mc_global.tty.use_subshell = FALSE;
    }
    tty_disable_interrupt_key ();
    if (!subshell_alive)
        mc_global.tty.use_subshell = FALSE;     /* Subshell died instantly, so don't use it */
}

/* --------------------------------------------------------------------------------------------- */

int
invoke_subshell (const char *command, int how, vfs_path_t ** new_dir_vpath)
{
    /* Make the MC terminal transparent */
    tcsetattr (STDOUT_FILENO, TCSANOW, &raw_mode);

    /* Make the subshell change to MC's working directory */
    if (new_dir_vpath != NULL)
        do_subshell_chdir (subshell_get_cwd_from_current_panel (), TRUE);

    if (command == NULL)        /* The user has done "C-o" from MC */
    {
        if (subshell_state == INACTIVE)
        {
            subshell_state = ACTIVE;
            /* FIXME: possibly take out this hack; the user can
               re-play it by hitting C-hyphen a few times! */
            if (subshell_ready)
                write_all (mc_global.tty.subshell_pty, " \b", 2);       /* Hack to make prompt reappear */
        }
    }
    else                        /* MC has passed us a user command */
    {
        if (how == QUIETLY)
            write_all (mc_global.tty.subshell_pty, " ", 1);
        /* FIXME: if command is long (>8KB ?) we go comma */
        write_all (mc_global.tty.subshell_pty, command, strlen (command));
        write_all (mc_global.tty.subshell_pty, "\n", 1);
        subshell_state = RUNNING_COMMAND;
        subshell_ready = FALSE;
    }

    feed_subshell (how, FALSE);

    if (new_dir_vpath != NULL && subshell_alive)
    {
        const char *pcwd;

        pcwd = vfs_translate_path (vfs_path_as_str (subshell_get_cwd_from_current_panel ()));
        if (strcmp (subshell_cwd, pcwd) != 0)
            *new_dir_vpath = vfs_path_from_str (subshell_cwd);  /* Make MC change to the subshell's CWD */
    }

    /* Restart the subshell if it has died by SIGHUP, SIGQUIT, etc. */
    while (!subshell_alive && subshell_get_mainloop_quit () == 0 && mc_global.tty.use_subshell)
        init_subshell ();

    return subshell_get_mainloop_quit ();
}


/* --------------------------------------------------------------------------------------------- */

gboolean
read_subshell_prompt (void)
{
    int rc = 0;
    ssize_t bytes = 0;
    struct timeval timeleft = { 0, 0 };
    GString *p;
    gboolean prompt_was_reset = FALSE;

    fd_set tmp;
    FD_ZERO (&tmp);
    FD_SET (mc_global.tty.subshell_pty, &tmp);

    /* First time through */
    if (subshell_prompt == NULL)
        subshell_prompt = g_string_sized_new (INITIAL_PROMPT_SIZE);

    p = g_string_sized_new (INITIAL_PROMPT_SIZE);

    while (subshell_alive
           && (rc = select (mc_global.tty.subshell_pty + 1, &tmp, NULL, NULL, &timeleft)) != 0)
    {
        ssize_t i;

        /* Check for 'select' errors */
        if (rc == -1)
        {
            if (errno == EINTR)
            {
                if (mc_global.tty.winch_flag != 0)
                    tty_change_screen_size ();

                continue;
            }

            fprintf (stderr, "select (FD_SETSIZE, &tmp...): %s\r\n", unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        bytes = read (mc_global.tty.subshell_pty, pty_buffer, sizeof (pty_buffer));

        /* Extract the prompt from the shell output */
        for (i = 0; i < bytes; i++)
            if (pty_buffer[i] == '\n' || pty_buffer[i] == '\r')
            {
                g_string_set_size (p, 0);
                prompt_was_reset = TRUE;
            }
            else if (pty_buffer[i] != '\0')
                g_string_append_c (p, pty_buffer[i]);
    }

    if (p->len != 0 || prompt_was_reset)
        g_string_assign (subshell_prompt, p->str);

    g_string_free (p, TRUE);

    return (rc != 0 || bytes != 0);
}

/* --------------------------------------------------------------------------------------------- */

void
do_update_prompt (void)
{
    if (update_subshell_prompt)
    {
        printf ("\r\n%s", subshell_prompt->str);
        fflush (stdout);
        update_subshell_prompt = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
exit_subshell (void)
{
    gboolean subshell_quit = TRUE;

    if (subshell_state != INACTIVE && subshell_alive)
        subshell_quit =
            query_dialog (_("Warning"),
                          _("The shell is still active. Quit anyway?"),
                          D_NORMAL, 2, _("&Yes"), _("&No")) == 0;

    if (subshell_quit)
    {
        if (mc_global.shell->type == SHELL_TCSH)
        {
            if (unlink (tcsh_fifo) == -1)
                fprintf (stderr, "Cannot remove named pipe %s: %s\r\n",
                         tcsh_fifo, unix_error_string (errno));
        }

        g_string_free (subshell_prompt, TRUE);
        subshell_prompt = NULL;
        pty_buffer[0] = '\0';
    }

    return subshell_quit;
}

/* --------------------------------------------------------------------------------------------- */

/** If it actually changed the directory it returns true */
void
do_subshell_chdir (const vfs_path_t * vpath, gboolean update_prompt)
{
    char *pcwd;

    pcwd = vfs_path_to_str_flags (subshell_get_cwd_from_current_panel (), 0, VPF_RECODE);

    if (!(subshell_state == INACTIVE && strcmp (subshell_cwd, pcwd) != 0))
    {
        /* We have to repaint the subshell prompt if we read it from
         * the main program.  Please note that in the code after this
         * if, the cd command that is sent will make the subshell
         * repaint the prompt, so we don't have to paint it. */
        if (update_prompt)
            do_update_prompt ();
        g_free (pcwd);
        return;
    }

    /* The initial space keeps this out of the command history (in bash
       because we set "HISTCONTROL=ignorespace") */
    write_all (mc_global.tty.subshell_pty, " cd ", 4);

    if (vpath != NULL)
    {
        const char *translate;

        translate = vfs_translate_path (vfs_path_as_str (vpath));
        if (translate != NULL)
        {
            GString *temp;

            temp = subshell_name_quote (translate);
            write_all (mc_global.tty.subshell_pty, temp->str, temp->len);
            g_string_free (temp, TRUE);
        }
        else
        {
            write_all (mc_global.tty.subshell_pty, ".", 1);
        }
    }
    else
    {
        write_all (mc_global.tty.subshell_pty, "/", 1);
    }
    write_all (mc_global.tty.subshell_pty, "\n", 1);

    subshell_state = RUNNING_COMMAND;
    feed_subshell (QUIETLY, FALSE);

    if (subshell_alive)
    {
        gboolean bPathNotEq;

        bPathNotEq = strcmp (subshell_cwd, pcwd) != 0;

        if (bPathNotEq && mc_global.shell->type == SHELL_TCSH)
        {
            char rp_subshell_cwd[PATH_MAX];
            char rp_current_panel_cwd[PATH_MAX];
            char *p_subshell_cwd, *p_current_panel_cwd;

            p_subshell_cwd = mc_realpath (subshell_cwd, rp_subshell_cwd);
            p_current_panel_cwd = mc_realpath (pcwd, rp_current_panel_cwd);

            if (p_subshell_cwd == NULL)
                p_subshell_cwd = subshell_cwd;
            if (p_current_panel_cwd == NULL)
                p_current_panel_cwd = pcwd;
            bPathNotEq = strcmp (p_subshell_cwd, p_current_panel_cwd) != 0;
        }

        if (bPathNotEq && !DIR_IS_DOT (pcwd))
        {
            char *cwd;

            cwd =
                vfs_path_to_str_flags (subshell_get_cwd_from_current_panel (), 0,
                                       VPF_STRIP_PASSWORD);
            vfs_print_message (_("Warning: Cannot change to %s.\n"), cwd);
            g_free (cwd);
        }
    }

    /* Really escape Zsh history */
    if (mc_global.shell->type == SHELL_ZSH)
    {
        /* Per Zsh documentation last command prefixed with space lingers in the internal history
         * until the next command is entered before it vanishes. To make it vanish right away,
         * type a space and press return. */
        write_all (mc_global.tty.subshell_pty, " \n", 2);
        subshell_state = RUNNING_COMMAND;
        feed_subshell (QUIETLY, FALSE);
    }

    update_subshell_prompt = FALSE;

    g_free (pcwd);
    /* Make sure that MC never stores the CWD in a silly format */
    /* like /usr////lib/../bin, or the strcmp() above will fail */
}

/* --------------------------------------------------------------------------------------------- */

void
subshell_get_console_attributes (void)
{
    /* Get our current terminal modes */

    if (tcgetattr (STDOUT_FILENO, &shell_mode))
    {
        fprintf (stderr, "Cannot get terminal settings: %s\r\n", unix_error_string (errno));
        mc_global.tty.use_subshell = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Figure out whether the subshell has stopped, exited or been killed
 * Possibly modifies: 'subshell_alive', 'subshell_stopped' and 'quit' */

void
sigchld_handler (int sig)
{
    int status;
    pid_t pid;

    (void) sig;

    pid = waitpid (subshell_pid, &status, WUNTRACED | WNOHANG);

    if (pid == subshell_pid)
    {
        /* Figure out what has happened to the subshell */

        if (WIFSTOPPED (status))
        {
            if (WSTOPSIG (status) == SIGSTOP)
            {
                /* The subshell has received a SIGSTOP signal */
                subshell_stopped = TRUE;
            }
            else
            {
                /* The user has suspended the subshell.  Revive it */
                kill (subshell_pid, SIGCONT);
            }
        }
        else
        {
            /* The subshell has either exited normally or been killed */
            subshell_alive = FALSE;
            delete_select_channel (mc_global.tty.subshell_pty);
            if (WIFEXITED (status) && WEXITSTATUS (status) != FORK_FAILURE)
            {
                int subshell_quit;
                subshell_quit = subshell_get_mainloop_quit () | SUBSHELL_EXIT;  /* Exited normally */
                subshell_set_mainloop_quit (subshell_quit);
            }
        }
    }
    subshell_handle_cons_saver ();

    /* If we got here, some other child exited; ignore it */
}

/* --------------------------------------------------------------------------------------------- */
