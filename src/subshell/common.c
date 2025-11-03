/*
   Concurrent shell support for the Midnight Commander

   Copyright (C) 1994-2025
   Free Software Foundation, Inc.

   Written by:
   Alexander Kriegisch <Alexander@Kriegisch.name>
   Aliaksey Kandratsenka <alk@tut.by>
   Andreas Mohr <and@gmx.li>
   Andrew Borodin <aborodin@vmail.ru>
   Andrew V. Samoilov <sav@bcs.zp.ua>
   Chris Owen <chris@candu.co.uk>
   Claes Nästén <me@pekdon.net>
   Egmont Koblinger <egmont@gmail.com>
   Enrico Weigelt, metux IT service <weigelt@metux.de>
   Eric Roberts <ericmrobertsdeveloper@gmail.com>
   Igor Urazov <z0rc3r@gmail.com>
   Ilia Maslakov <il.smind@gmail.com>
   Leonard den Ottolander <leonard@den.ottolander.nl>
   Miguel de Icaza <miguel@novell.com>
   Mikhail S. Pobolovets <styx.mp@gmail.com>
   Norbert Warmuth <nwarmuth@privat.circular.de>
   Oswald Buddenhagen <oswald.buddenhagen@gmx.de>
   Patrick Winnertz <winnie@debian.org>
   Pavel Machek <pavel@suse.cz>
   Pavel Roskin <proski@gnu.org>
   Pavel Tsekov <ptsekov@gmx.net>
   Roland Illig <roland.illig@gmx.de>
   Sergei Trofimovich <slyfox@inbox.ru>
   Slava Zanko <slavazanko@gmail.com>
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <termios.h>

#ifdef HAVE_STROPTS_H
#include <stropts.h>  // For I_PUSH
#endif

#ifdef HAVE_OPENPTY
/* includes for openpty() */
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
/* <sys/types.h> is a prerequisite of <libutil.h> on FreeBSD 8.0.  */
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#endif

#include "lib/global.h"

#include "lib/fileloc.h"
#include "lib/terminal.h"
#include "lib/unixcompat.h"
#include "lib/tty/tty.h"  // LINES
#include "lib/tty/key.h"  // XCTRL
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/widget.h"

#include "src/filemanager/layout.h"   // setup_cmdline()
#include "src/filemanager/command.h"  // cmdline

#include "subshell.h"
#include "internal.h"

/*** global variables ****************************************************************************/

/* State of the subshell:
 * INACTIVE: the default state; awaiting a command
 * ACTIVE: remain in the shell until the user hits the subshell switch key
 * RUNNING_COMMAND: return to MC when the current command finishes */
enum subshell_state_enum subshell_state;

/* Holds the latest prompt captured from the subshell */
GString *subshell_prompt = NULL;

/* Subshell: if set, then the prompt was not saved on CONSOLE_SAVE */
/* We need to paint it after CONSOLE_RESTORE, see: load_prompt */
gboolean update_subshell_prompt = FALSE;

/* If set, then a command has just finished executing, and we need */
/* to be on the lookout for a new prompt string from the subshell. */
gboolean should_read_new_subshell_prompt;

/*** file scope macro definitions ****************************************************************/

#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned) (stat_val) >> 8)
#endif

#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

/* Initial length of the buffer for the subshell's prompt */
#define INITIAL_PROMPT_SIZE 10

/* Used by the child process to indicate failure to start the subshell */
#define FORK_FAILURE 69  // Arbitrary

/* Length of the buffer for all I/O with the subshell */
#define PTY_BUFFER_SIZE BUF_MEDIUM  // Arbitrary; but keep it >= 80

/*** file scope type declarations ****************************************************************/

/* For pipes */
enum
{
    READ = 0,
    WRITE = 1
};

typedef enum
{
    MODIFIER_SHIFT = 0x01,
    MODIFIER_CTRL = 0x04,
    MODIFIER_CAPS_LOCK = 0x40,
    MODIFIER_NUM_LOCK = 0x80,

    EVENT_TYPE_PRESS = 1,
    EVENT_TYPE_REPEAT = 2,
} kitty_keyboard_protocol_t;

/* This is the keybinding that is sent to the shell, to make the shell send us the contents of
 * the current command buffer and the location of the cursor. */
#define SHELL_BUFFER_KEYBINDING "_"

/*** forward declarations (file scope functions) *************************************************/

/*** file scope variables ************************************************************************/

/* tcsh closes all non-standard file descriptors, so we have to use a pipe */
static char tcsh_fifo[BUF_SMALL];

static int subshell_pty_slave = -1;

/* For reading/writing on the subshell's pty */
static char pty_buffer[PTY_BUFFER_SIZE] = "\0";

/* To pass CWD info from the subshell to MC */
static int subshell_pipe[2];

/* To pass command buffer info from the subshell to MC */
static int command_buffer_pipe[2];

/* The subshell's process ID */
static pid_t subshell_pid = 1;

/* One extra char for final '\n' */
static char subshell_cwd[MC_MAXPATHLEN + 1];

/* Flag to indicate whether the subshell is ready for next command */
static int subshell_ready;

/* Flag to indicate if the subshell supports the persistent buffer feature. */
static gboolean use_persistent_buffer = FALSE;

/* This is the local variable where the subshell prompt is stored while we are working on it. */
static GString *subshell_prompt_temp_buffer = NULL;

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
        const ssize_t ret = write (fd, (const unsigned char *) buf + written, count);

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                if (tty_got_winch ())
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
    pid_t mc_sid;

    (void) pty_name;
    setsid ();  // Get a fresh terminal session

    // Make sure that it has become our controlling terminal

    // Redundant on Linux and probably most systems, but just in case:

#ifdef TIOCSCTTY
    ioctl (subshell_pty_slave, TIOCSCTTY, 0);
#endif

    // Configure its terminal modes and window size

    // Set up the pty with the same termios flags as our own tty
    if (tcsetattr (subshell_pty_slave, TCSANOW, &shell_mode))
    {
        fprintf (stderr, "Cannot set pty terminal modes: %s\r\n", unix_error_string (errno));
        my_exit (FORK_FAILURE);
    }

    // Set the pty's size (80x25 by default on Linux) according to the
    // size of the real terminal as calculated by ncurses, if possible
    tty_resize (subshell_pty_slave);

    // Set up the subshell's environment and init file name

    // It simplifies things to change to our home directory here,
    // and the user's startup file may do a 'cd' command anyway
    // FIXME? What about when we re-run the subshell?
    MC_UNUSED const int ret_chdir = chdir (mc_config_get_home_dir ());

    // Set MC_SID to prevent running one mc from another
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
        // Do we have a custom init file ~/.local/share/mc/bashrc?
        init_file = mc_config_get_full_path (MC_BASHRC_CUSTOM_PROFILE_FILE);

        // Otherwise use ~/.bashrc
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (MC_BASHRC_DEFAULT_PROFILE_FILE);
        }

        /* Make MC's special commands not show up in bash's history and also suppress
         * consecutive identical commands*/
        putenv ((char *) "HISTCONTROL=ignoreboth");

        // Allow alternative readline settings for MC
        {
            char *input_file;

            input_file = mc_config_get_full_path (MC_INPUTRC_FILE);
            if (exist_file (input_file))
                g_setenv ("INPUTRC", input_file, TRUE);
            g_free (input_file);
        }

        break;

    case SHELL_ASH_BUSYBOX:
    case SHELL_DASH:
        // Do we have a custom init file ~/.local/share/mc/ashrc?
        init_file = mc_config_get_full_path (MC_ASHRC_CUSTOM_PROFILE_FILE);

        // Otherwise use ~/.profile
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (MC_GENERIC_DEFAULT_PROFILE_FILE);
        }

        /* Put init file to ENV variable used by ash but only if it
           is not already set. */
        g_setenv ("ENV", init_file, FALSE);

        break;

    case SHELL_KSH:
        // Do we have a custom init file ~/.local/share/mc/kshrc?
        init_file = mc_config_get_full_path (MC_KSHRC_CUSTOM_PROFILE_FILE);

        // Otherwise use ~/.profile
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (MC_GENERIC_DEFAULT_PROFILE_FILE);
        }

        /* Put init file to ENV variable used by ksh but only if it
         * is not already set. */
        g_setenv ("ENV", init_file, FALSE);

        // Make MC's special commands not show up in history
        putenv ((char *) "HISTCONTROL=ignorespace");

        break;

    case SHELL_MKSH:
        // Do we have a custom init file ~/.local/share/mc/mkshrc?
        init_file = mc_config_get_full_path (MC_MKSHRC_CUSTOM_PROFILE_FILE);

        // Otherwise use ~/.mkshrc (default behavior of mksh)
        if (!exist_file (init_file))
        {
            g_free (init_file);
            init_file = g_strdup (MC_MKSHRC_DEFAULT_PROFILE_FILE);
        }

        /* Put init file to ENV variable used by mksh but only if it
         * is not already set. */
        g_setenv ("ENV", init_file, FALSE);

        // Note mksh doesn't support HISTCONTROL.

        break;

    case SHELL_ZSH:
        /* ZDOTDIR environment variable is the only way to point zsh
         * to an other rc file than the default. */

        // Don't overwrite $ZDOTDIR
        if (g_getenv ("ZDOTDIR") != NULL)
        {
            /* Do we have a custom init file ~/.local/share/mc/.zshrc?
             * Otherwise use standard ~/.zshrc */
            init_file = mc_config_get_full_path (MC_ZSHRC_CUSTOM_PROFILE_FILE);
            if (exist_file (init_file))
            {
                // Set ZDOTDIR to ~/.local/share/mc
                g_setenv ("ZDOTDIR", mc_config_get_data_path (), TRUE);
            }
        }
        break;

        // TODO: Find a way to pass initfile to TCSH and FISH
    case SHELL_TCSH:
    case SHELL_FISH:
        break;

    default:
        fprintf (stderr, __FILE__ ": unimplemented subshell type %u\r\n", mc_global.shell->type);
        my_exit (FORK_FAILURE);
    }

    // Attach all our standard file descriptors to the pty

    // This is done just before the exec, because stderr must still
    // be connected to the real tty during the above error messages;
    // otherwise the user will never see them.

    dup2 (subshell_pty_slave, STDIN_FILENO);
    dup2 (subshell_pty_slave, STDOUT_FILENO);
    dup2 (subshell_pty_slave, STDERR_FILENO);

    close (subshell_pipe[READ]);

    if (use_persistent_buffer)
        close (command_buffer_pipe[READ]);

    close (subshell_pty_slave);  // These may be FD_CLOEXEC, but just in case...
    // Close master side of pty.  This is important; apart from
    // freeing up the descriptor for use in the subshell, it also
    // means that when MC exits, the subshell will get a SIGHUP and
    // exit too, because there will be no more descriptors pointing
    // at the master side of the pty and so it will disappear.
    close (mc_global.tty.subshell_pty);

    // Execute the subshell at last

    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
        execl (mc_global.shell->path, mc_global.shell->path, "--rcfile", init_file, (char *) NULL);
        break;

    case SHELL_ZSH:
        /* Use -g to exclude cmds beginning with space from history
         * and -Z to use the line editor on non-interactive term */
        execl (mc_global.shell->path, mc_global.shell->path, "-Z", "-g", (char *) NULL);
        break;

    case SHELL_FISH:
        execl (mc_global.shell->path, mc_global.shell->path, "--init-command",
               "set --global __mc_kitty_keyboard 1", (char *) NULL);
        break;

    case SHELL_ASH_BUSYBOX:
    case SHELL_DASH:
    case SHELL_TCSH:
    case SHELL_KSH:
    case SHELL_MKSH:
        execl (mc_global.shell->path, mc_global.shell->path, (char *) NULL);
        break;

    default:
        break;
    }

    // If we get this far, everything failed miserably
    g_free (init_file);
    my_exit (FORK_FAILURE);
}

/* --------------------------------------------------------------------------------------------- */

static void
init_raw_mode (void)
{
    static gboolean initialized = FALSE;

    // MC calls tty_reset_shell_mode() in pre_exec() to set the real tty to its
    // original settings.  However, here we need to make this tty very raw,
    // so that all keyboard signals, XON/XOFF, etc. will get through to the
    // pty.  So, instead of changing the code for execute(), pre_exec(),
    // etc, we just set up the modes we need here, before each command.

    if (!initialized)  // First time: initialise 'raw_mode'
    {
        tcgetattr (STDOUT_FILENO, &raw_mode);
        raw_mode.c_lflag &= ~ICANON;  // Disable line-editing chars, etc.
        raw_mode.c_lflag &= ~ISIG;    // Disable intr, quit & suspend chars
        raw_mode.c_lflag &= ~ECHO;    // Disable input echoing
        raw_mode.c_iflag &= ~IXON;    // Pass ^S/^Q to subshell undisturbed
        raw_mode.c_iflag &= ~ICRNL;   // Don't translate CRs into LFs
        raw_mode.c_oflag &= ~OPOST;   // Don't postprocess output
        raw_mode.c_cc[VTIME] = 0;     // IE: wait forever, and return as
        raw_mode.c_cc[VMIN] = 1;      // soon as a character is available
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

    // Wait until the subshell has stopped
    while (subshell_alive && !subshell_stopped)
    {
        // On macOS, sigsuspend() may fail to restore the original signal mask.
        // https://openradar.appspot.com/FB18565075
        pselect (0, NULL, NULL, NULL, NULL, &old_mask);
    }

    if (subshell_state != ACTIVE)
    {
        // Discard all remaining data from stdin to the subshell
        tcflush (subshell_pty_slave, TCIFLUSH);
    }

    subshell_stopped = FALSE;
    kill (subshell_pid, SIGCONT);

    sigprocmask (SIG_SETMASK, &old_mask, NULL);
    // We can't do any better without modifying the shell(s)
}

/* --------------------------------------------------------------------------------------------- */
/** Get the contents of the current subshell command line buffer and
 * transfer the contents to the panel command prompt.
 */

static gboolean
read_command_line_buffer (gboolean test_mode)
{
    char subshell_response_buffer[BUF_LARGE];
    size_t response_char_length = 0;  // in bytes, up to (excluding) '\0'

    char *subshell_command;
    int command_length;  // in characters

    fd_set read_set;
    ssize_t bytes;
    struct timeval subshell_prompt_timer = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    int bash_version;
    int cursor_position;
    int rc;

    if (!use_persistent_buffer)
        return TRUE;

    FD_ZERO (&read_set);
    FD_SET (command_buffer_pipe[READ], &read_set);

    const int maxfdp = command_buffer_pipe[READ];

    /* First, flush the command buffer pipe. This pipe shouldn't be written
     * to under normal circumstances, but if it somehow does get written
     * to, we need to make sure to discard whatever data is there before
     * we try to use it. */
    while ((rc = select (maxfdp + 1, &read_set, NULL, NULL, &subshell_prompt_timer)) != 0)
    {
        if (rc == -1)
        {
            if (errno == EINTR)
                continue;

            return FALSE;
        }

        if (rc == 1)
        {
            bytes = read (command_buffer_pipe[READ], subshell_response_buffer,
                          sizeof (subshell_response_buffer));
            (void) bytes;
        }
    }

    // Query the cursor position and the contents of the command line buffer
    write_all (mc_global.tty.subshell_pty, ESC_STR SHELL_BUFFER_KEYBINDING,
               sizeof (ESC_STR SHELL_BUFFER_KEYBINDING) - 1);

    // Read the response
    subshell_prompt_timer.tv_sec = 1;
    FD_ZERO (&read_set);
    FD_SET (command_buffer_pipe[READ], &read_set);

    while (TRUE)
    {
        rc = select (maxfdp + 1, &read_set, NULL, NULL, &subshell_prompt_timer);

        if (rc == -1)
        {
            if (errno == EINTR)
                continue;

            return FALSE;
        }

        if (rc == 0)
            return FALSE;

        bytes = read (command_buffer_pipe[READ], subshell_response_buffer + response_char_length,
                      sizeof (subshell_response_buffer) - response_char_length);
        if (bytes <= 0
            || (size_t) bytes == sizeof (subshell_response_buffer) - response_char_length)
            return FALSE;

        // Did we receive the terminating '\0'? There shouldn't be an embedded '\0', but just in
        // case there is, stop at the first one.
        const int latest_chunk_data_length =
            strnlen (subshell_response_buffer + response_char_length, bytes);
        if (latest_chunk_data_length < bytes)
        {
            // Terminating '\0' found, we're done reading
            response_char_length += latest_chunk_data_length;
            break;
        }
        // No terminating '\0' yet, keep reading
        response_char_length += bytes;
    }

    // fish sends a '\n' before the terminating '\0', strip it
    if (mc_global.shell->type == SHELL_FISH)
    {
        if (response_char_length == 0)
            return FALSE;
        if (subshell_response_buffer[response_char_length - 1] != '\n')
            return FALSE;
        subshell_response_buffer[--response_char_length] = '\0';
    }

    // bash sends two numbers in the first line, fish and zsh send one
    if (mc_global.shell->type == SHELL_BASH)
    {
        if (sscanf (subshell_response_buffer, "%d:%d", &bash_version, &cursor_position) != 2)
            return FALSE;
    }
    else
    {
        if (sscanf (subshell_response_buffer, "%d", &cursor_position) != 1)
            return FALSE;
        bash_version = 1000;
    }

    // Locate the second line of the response which contains the subshell command
    subshell_command = strchr (subshell_response_buffer, '\n');
    if (subshell_command == NULL)
        return FALSE;
    subshell_command++;
    command_length = str_length (subshell_command);

    // The response was valid
    if (test_mode)
        return TRUE;

    /* Substitute non-text characters in the command buffer, such as tab, or newline, as this
     * could cause problems. */
    for (unsigned char *p = (unsigned char *) subshell_command; *p != '\0'; p++)
        if (*p < 32 || *p == 127)
            *p = ' ';

    input_assign_text (cmdline, "");
    input_insert (cmdline, subshell_command, FALSE);

    if (bash_version < 5)  // implies SHELL_BASH
    {
        /* We need to do this because bash < v5 gives the cursor position in a UTF-8 string based
         * on the location in bytes, not in Unicode characters. */
        char *curr, *stop;

        curr = subshell_command;
        stop = curr + cursor_position;

        for (cursor_position = 0; curr < stop; cursor_position++)
            str_next_char_safe (&curr);
    }
    if (cursor_position > command_length)
        cursor_position = command_length;
    cmdline->point = cursor_position;

    // We send any remaining data to STDOUT before we finish.
    flush_subshell (0, VISIBLY);

    // Now we erase the current contents of the command line buffer
    if (mc_global.shell->type != SHELL_ZSH)
    {
        /* In zsh, we can just press c-u to clear the line, without needing to go to the end of
         * the line first. In all other shells, we must go to the end of the line first. */

        // If we are not at the end of the line, we go to the end
        if (cursor_position != command_length)
        {
            write_all (mc_global.tty.subshell_pty, "\005", 1);
            if (flush_subshell (1, VISIBLY) != 1)
                return FALSE;
        }
    }

    if (command_length > 0)
    {
        // Now we clear the line
        write_all (mc_global.tty.subshell_pty, "\025", 1);
        if (flush_subshell (1, VISIBLY) != 1)
            return FALSE;
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- */

static void
clear_subshell_prompt_string (void)
{
    if (subshell_prompt_temp_buffer != NULL)
        g_string_set_size (subshell_prompt_temp_buffer, 0);
}

/* --------------------------------------------------------------------------------------------- */

static void
parse_subshell_prompt_string (const char *buffer, size_t bytes)
{
    if (mc_global.mc_run_mode != MC_RUN_FULL)
        return;

    // First time through
    if (subshell_prompt == NULL)
        subshell_prompt = g_string_sized_new (INITIAL_PROMPT_SIZE);
    if (subshell_prompt_temp_buffer == NULL)
        subshell_prompt_temp_buffer = g_string_sized_new (INITIAL_PROMPT_SIZE);

    // Extract the prompt from the shell output
    for (size_t i = 0; i < bytes; i++)
        if (buffer[i] == '\n' || buffer[i] == '\r')
            g_string_set_size (subshell_prompt_temp_buffer, 0);
        else if (buffer[i] != '\0')
            g_string_append_c (subshell_prompt_temp_buffer, buffer[i]);
}

/* --------------------------------------------------------------------------------------------- */

static void
set_prompt_string (void)
{
    if (mc_global.mc_run_mode != MC_RUN_FULL)
        return;

    if (subshell_prompt_temp_buffer->len != 0)
        mc_g_string_copy (subshell_prompt, subshell_prompt_temp_buffer);

    setup_cmdline ();
}

/* --------------------------------------------------------------------------------------------- */

static gboolean
peek_subshell_switch_key (const char *buffer, size_t len)
{
    csi_command_t csi;

    if (len == 0)
        return FALSE;
    if (buffer[0] == (XCTRL ('o') & 255))
        return TRUE;

    // Also check if ctrl-o is encoded as per the kitty keyboard protocol.
    if (len == 1)
        return FALSE;
    if (buffer[0] != ESC_CHAR || buffer[1] != '[')  // CSI
        return FALSE;

    buffer += 2;
    len -= 2;

    if (!parse_csi (&csi, &buffer, buffer + len))
        return FALSE;
    if (csi.private_mode != '\0' || buffer[-1] != 'u')
        return FALSE;
    if (csi.param_count != 2)  // ctrl-o must have the modifier field
        return FALSE;
    if (csi.params[1][0] == 0)  // Bad modifier.
        return FALSE;

    const uint32_t codepoint = csi.params[0][0];
    const uint32_t modifiers = csi.params[1][0] - 1;
    const uint32_t event = csi.params[1][1];

    if (event != 0 && event != EVENT_TYPE_PRESS && event != EVENT_TYPE_REPEAT)
        return FALSE;

    return codepoint == 'o'
        && (modifiers & ~(MODIFIER_CAPS_LOCK | MODIFIER_NUM_LOCK)) == MODIFIER_CTRL;
}

/* --------------------------------------------------------------------------------------------- */
/** Feed the subshell our keyboard input until it says it's finished */

static gboolean
feed_subshell (int how, gboolean fail_on_error)
{
    fd_set read_set;  // For 'select'

    struct timeval wtime;  // Maximum time we wait for the subshell
    struct timeval *wptr;

    should_read_new_subshell_prompt = FALSE;

    /* have more than enough time to run subshell:
       wait up to 10 second if fail_on_error, forever otherwise */
    wtime.tv_sec = 10;
    wtime.tv_usec = 0;
    wptr = fail_on_error ? &wtime : NULL;

    while (TRUE)
    {
        int maxfdp;

        if (!subshell_alive)
            return FALSE;

        // Prepare the file-descriptor set and call 'select'

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
            // Despite using SA_RESTART, we still have to check for this
            if (errno == EINTR)
            {
                if (tty_got_winch ())
                    tty_change_screen_size ();

                continue;  // try all over again
            }
            tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
            fprintf (stderr, "select (FD_SETSIZE, &read_set...): %s\r\n",
                     unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        if (FD_ISSET (mc_global.tty.subshell_pty, &read_set))
        // Read from the subshell, write to stdout

        /* This loop improves performance by reducing context switches
           by a factor of 20 or so... unfortunately, it also hangs MC
           randomly, because of an apparent Linux bug.  Investigate. */
        // for (i=0; i<5; ++i)  * FIXME -- experimental
        {
            const ssize_t bytes =
                read (mc_global.tty.subshell_pty, pty_buffer, sizeof (pty_buffer));

            // The subshell has died
            if (bytes == -1 && errno == EIO && !subshell_alive)
                return FALSE;

            if (bytes <= 0)
            {
#ifdef PTY_ZEROREAD
                // On IBM i, read(1) can return 0 for a non-closed fd
                continue;
#else
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr, "read (subshell_pty...): %s\r\n", unix_error_string (errno));
                exit (EXIT_FAILURE);
#endif
            }

            if (how == VISIBLY)
                write_all (STDOUT_FILENO, pty_buffer, (size_t) bytes);

            if (should_read_new_subshell_prompt)
                parse_subshell_prompt_string (pty_buffer, (size_t) bytes);
        }

        else if (FD_ISSET (subshell_pipe[READ], &read_set))
        // Read the subshell's CWD and capture its prompt
        {
            const ssize_t bytes = read (subshell_pipe[READ], subshell_cwd, sizeof (subshell_cwd));

            if (bytes <= 0)
            {
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr, "read (subshell_pipe[READ]...): %s\r\n",
                         unix_error_string (errno));
                exit (EXIT_FAILURE);
            }

            subshell_cwd[(size_t) bytes - 1] = '\0';  // Squash the final '\n'

            synchronize ();

            clear_subshell_prompt_string ();
            should_read_new_subshell_prompt = TRUE;
            subshell_ready = TRUE;
            if (subshell_state == RUNNING_COMMAND)
            {
                subshell_state = INACTIVE;
                return TRUE;
            }
        }

        else if (FD_ISSET (STDIN_FILENO, &read_set))
        // Read from stdin, write to the subshell
        {
            should_read_new_subshell_prompt = FALSE;

            const ssize_t bytes = read (STDIN_FILENO, pty_buffer, sizeof (pty_buffer));

            if (bytes <= 0)
            {
                tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
                fprintf (stderr, "read (STDIN_FILENO, pty_buffer...): %s\r\n",
                         unix_error_string (errno));
                exit (EXIT_FAILURE);
            }

            // just type casting
            const size_t ubytes = (size_t) bytes;

            for (size_t i = 0; i < ubytes; i++)
                if (peek_subshell_switch_key (pty_buffer + i, ubytes - i))
                {
                    write_all (mc_global.tty.subshell_pty, pty_buffer, i);

                    if (subshell_ready)
                    {
                        subshell_state = INACTIVE;
                        set_prompt_string ();
                        if (subshell_ready && !read_command_line_buffer (FALSE))
                        {
                            // If we got here, some unforeseen error must have occurred.
                            if (mc_global.shell->type != SHELL_FISH)
                            {
                                write_all (mc_global.tty.subshell_pty, "\003", 1);
                                subshell_state = RUNNING_COMMAND;
                                if (feed_subshell (QUIETLY, TRUE)
                                    && read_command_line_buffer (FALSE))
                                    return TRUE;
                            }

                            subshell_state = ACTIVE;
                            flush_subshell (0, VISIBLY);
                            input_assign_text (cmdline, "");
                        }
                    }

                    return TRUE;
                }

            write_all (mc_global.tty.subshell_pty, pty_buffer, ubytes);

            if (pty_buffer[ubytes - 1] == '\n' || pty_buffer[ubytes - 1] == '\r')
            {
                /* We should only clear the command line if we are using a shell that works
                 * with persistent command buffer, otherwise we get awkward results. */
                if (use_persistent_buffer)
                    input_assign_text (cmdline, "");
                subshell_ready = FALSE;
            }
        }
        else
            return FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */
/* pty opening functions */

#ifndef HAVE_OPENPTY

#ifdef HAVE_GRANTPT

/* System V version of pty_open_master */

static int
pty_open_master (char *pty_name)
{
    char *slave_name;

#ifdef HAVE_POSIX_OPENPT
    const int pty_master = posix_openpt (O_RDWR);
#elif defined HAVE_GETPT
    // getpt () is a GNU extension (glibc 2.1.x)
    const int pty_master = getpt ();
#elif defined IS_AIX
    strcpy (pty_name, "/dev/ptc");
    const int pty_master = open (pty_name, O_RDWR);
#else
    strcpy (pty_name, "/dev/ptmx");
    const int pty_master = open (pty_name, O_RDWR);
#endif

    if (pty_master == -1)
        return -1;

    if (grantpt (pty_master) == -1                       // Grant access to slave
        || unlockpt (pty_master) == -1                   // Clear slave's lock flag
        || (slave_name = ptsname (pty_master)) == NULL)  // Get slave's name
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
    const int pty_slave = open (pty_name, O_RDWR);

    if (pty_slave == -1)
    {
        fprintf (stderr, "open (%s, O_RDWR): %s\r\n", pty_name, unix_error_string (errno));
        return -1;
    }
#if !defined(__osf__) && !defined(__linux__)
#if defined(I_FIND) && defined(I_PUSH)
    if (ioctl (pty_slave, I_FIND, "ptem") == 0 && ioctl (pty_slave, I_PUSH, "ptem") == -1)
    {
        fprintf (stderr, "ioctl (%d, I_PUSH, \"ptem\") failed: %s\r\n", pty_slave,
                 unix_error_string (errno));
        close (pty_slave);
        return -1;
    }

    if (ioctl (pty_slave, I_FIND, "ldterm") == 0 && ioctl (pty_slave, I_PUSH, "ldterm") == -1)
    {
        fprintf (stderr, "ioctl (%d, I_PUSH, \"ldterm\") failed: %s\r\n", pty_slave,
                 unix_error_string (errno));
        close (pty_slave);
        return -1;
    }
#if !defined(sgi) && !defined(__sgi)
    if (ioctl (pty_slave, I_FIND, "ttcompat") == 0 && ioctl (pty_slave, I_PUSH, "ttcompat") == -1)
    {
        fprintf (stderr, "ioctl (%d, I_PUSH, \"ttcompat\") failed: %s\r\n", pty_slave,
                 unix_error_string (errno));
        close (pty_slave);
        return -1;
    }
#endif
#endif
#endif

    fcntl (pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}

#else  // !HAVE_GRANTPT

/* --------------------------------------------------------------------------------------------- */
/** BSD version of pty_open_master */

static int
pty_open_master (char *pty_name)
{
    strcpy (pty_name, "/dev/ptyXX");

    for (const char *ptr1 = "pqrstuvwxyzPQRST"; *ptr1 != '\0'; ++ptr1)
    {
        pty_name[8] = *ptr1;

        for (const char *ptr2 = "0123456789abcdef"; *ptr2 != '\0'; ++ptr2)
        {
            pty_name[9] = *ptr2;

            // Try to open master
            const int pty_master = open (pty_name, O_RDWR);

            if (pty_master == -1)
            {
                if (errno == ENOENT)  // Different from EIO
                    return -1;        // Out of pty devices
                continue;             // Try next pty device
            }
            pty_name[5] = 't';  // Change "pty" to "tty"
            if (access (pty_name, 6) != 0)
            {
                close (pty_master);
                pty_name[5] = 'p';
                continue;
            }
            return pty_master;
        }
    }
    return -1;  // Ran out of pty devices
}

/* --------------------------------------------------------------------------------------------- */
/** BSD version of pty_open_slave */

static int
pty_open_slave (const char *pty_name)
{
    struct group *group_info = getgrnam ("tty");

    if (group_info != NULL)
    {
        // The following two calls will only succeed if we are root
        // [Commented out while permissions problem is investigated]
        // chown (pty_name, getuid (), group_info->gr_gid);  FIXME
        // chmod (pty_name, S_IRUSR | S_IWUSR | S_IWGRP);   FIXME
    }

    const int pty_slave = open (pty_name, O_RDWR);

    if (pty_slave == -1)
        fprintf (stderr, "open (pty_name, O_RDWR): %s\r\n", pty_name);
    fcntl (pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}
#endif

#endif

/* --------------------------------------------------------------------------------------------- */
/**
 * Set up `precmd' or equivalent for reading the subshell's CWD.
 *
 * Attention! Never forget that these are *one-liners* even though the concatenated
 * substrings contain line breaks and indentation for better understanding of the
 * shell code. It is vital that each one-liner ends with a line feed character ("\n" ).
 *
 * Also note that some shells support not remembering commands beginning with a space in their
 * history (HISTCONTROL=ignorespace or equivalent). Let's have leading spaces consistently
 * throughout the data we feed, even for shells that don't support it, it cannot hurt.
 *
 * @return initialized pre-command string
 */
static gchar *
init_subshell_precmd (void)
{
    /*
     * Fallback precmd emulation that should work with virtually any shell.
     *
     * Explanation of the indirect hop via $MC_PRECMD:
     *
     * Scenario: The user exports PS1 and then invokes a sub-subshell (e.g. "sh").
     *
     * This would lead to the sub-subshell stopping (=frozen mc):
     *     PS1='$(pwd >&%d; kill -STOP $$)...'
     *
     * This would lead to an error message like "sh: mc_precmd: not found":
     *     mc_precmd() { pwd >&%d; kill -STOP $$; }
     *     PS1='$(mc_precmd)...'
     *
     * A hop via $MC_PRECMD works because in the sub-subshell MC_PRECMD is undefined (assuming the
     * user did not export this one), thus evaluated to empty string - no damage done.
     */
    static const char *precmd_fallback = " mc_precmd() {"
                                         "   pwd >&%d;"
                                         "   kill -STOP $$;"
                                         " };"
                                         " MC_PRECMD=mc_precmd;"
                                         " PS1='$($MC_PRECMD)'\"$PS1\"\n";

    switch (mc_global.shell->type)
    {
    case SHELL_BASH:
        return g_strdup_printf (
            " mc_print_command_buffer () { printf '%%s:%%s\\n%%s\\000' \"$BASH_VERSINFO\" "
            "\"$READLINE_POINT\" \"$READLINE_LINE\" >&%d; }\n"
            " bind -x '\"\\e" SHELL_BUFFER_KEYBINDING "\":\"mc_print_command_buffer\"'\n"
            " if test $BASH_VERSINFO -ge 5 && [[ ${PROMPT_COMMAND@a} == *a* ]] 2> "
            "/dev/null; then\n"
            "   PROMPT_COMMAND+=( 'pwd >&%d; kill -STOP $$' )\n"
            " else\n"
            "   PROMPT_COMMAND=${PROMPT_COMMAND:+$PROMPT_COMMAND\n}'pwd >&%d; kill -STOP $$'\n"
            " fi\n",
            command_buffer_pipe[WRITE], subshell_pipe[WRITE], subshell_pipe[WRITE]);

    case SHELL_ASH_BUSYBOX:
        // BusyBox needs to be built with CONFIG_ASH_EXPAND_PRMT=y (this is the default)
        return g_strdup_printf (precmd_fallback, subshell_pipe[WRITE]);

    case SHELL_DASH:
        return g_strdup_printf (precmd_fallback, subshell_pipe[WRITE]);

    case SHELL_MKSH:
        return g_strdup_printf (precmd_fallback, subshell_pipe[WRITE]);

    case SHELL_KSH:
        return g_strdup_printf (" PS1='$(pwd >&%d; kill -STOP $$)'\"$PS1\"\n",
                                subshell_pipe[WRITE]);

    case SHELL_ZSH:
        return g_strdup_printf (
            " mc_print_command_buffer () { printf '%%s\\n%%s\\000' \"$CURSOR\" \"$BUFFER\" "
            ">&%d; }\n"
            " zle -N mc_print_command_buffer\n"
            " bindkey '^[" SHELL_BUFFER_KEYBINDING "' mc_print_command_buffer\n"
            " _mc_precmd() { pwd>&%d; kill -STOP $$; }\n"
            " precmd_functions+=(_mc_precmd)\n",
            command_buffer_pipe[WRITE], subshell_pipe[WRITE]);

    case SHELL_TCSH:
        // "echo -n" is a workaround against a suspected tcsh bug, see ticket #4120
        return g_strdup_printf (" set echo_style=both;"
                                " alias precmd 'echo -n; echo $cwd:q >%s; kill -STOP $$'\n",
                                tcsh_fifo);

    case SHELL_FISH:
        return g_strdup_printf (" bind \\e" SHELL_BUFFER_KEYBINDING
                                " \"begin; commandline -C; commandline; printf '\\000'; end >&%d\";"
                                " if not functions -q fish_prompt_mc;"
                                " functions -e fish_right_prompt;"
                                " functions -c fish_prompt fish_prompt_mc;"
                                " end;"
                                " function fish_prompt;"
                                " echo \"$PWD\" >&%d; kill -STOP $fish_pid; fish_prompt_mc;"
                                " end\n",
                                command_buffer_pipe[WRITE], subshell_pipe[WRITE]);
    default:
        fprintf (stderr, "subshell: unknown shell type (%u), aborting!\r\n", mc_global.shell->type);
        exit (EXIT_FAILURE);
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
 *   cd "`printf '%b' 'ABC\0nnnDEF\0nnnXYZ'`"
 *
 * N.B.: Use single quotes for conversion specifier to work around
 *       tcsh 6.20+ parser breakage, see ticket #3852 for the details.
 */

static GString *
subshell_name_quote (const char *s)
{
    GString *ret;
    const char *n;
    const char *quote_cmd_start, *quote_cmd_end;

    if (mc_global.shell->type == SHELL_FISH)
    {
        quote_cmd_start = "(printf '%b' '";
        quote_cmd_end = "')";
    }
    /* TODO: When BusyBox printf is fixed, get rid of this "else if", see
       https://lists.busybox.net/pipermail/busybox/2012-March/077460.html */
    /* else if (subshell_type == ASH_BUSYBOX)
       {
       quote_cmd_start = "\"`echo -en '";
       quote_cmd_end = "'`\"";
       } */
    else
    {
        quote_cmd_start = "\"`printf '%b' '";
        quote_cmd_end = "'`\"";
    }

    ret = g_string_sized_new (64);

    // Prevent interpreting leading '-' as a switch for 'cd'
    if (s[0] == '-')
        g_string_append (ret, "./");

    // Copy the beginning of the command to the buffer
    g_string_append (ret, quote_cmd_start);

    /*
     * Print every character except digits and letters as a backslash-escape
     * sequence of the form \0nnn, where "nnn" is the numeric value of the
     * character converted to octal number.
     */
    for (const char *su = s; su[0] != '\0'; su = n)
    {
        n = str_cget_next_char_safe (su);

        if (str_isalnum (su))
            g_string_append_len (ret, su, (size_t) (n - su));
        else
            for (size_t c = 0; c < (size_t) (n - su); c++)
                g_string_append_printf (ret, "\\0%03o", (unsigned char) su[c]);
    }

    g_string_append (ret, quote_cmd_end);

    return ret;
}

/* --------------------------------------------------------------------------------------------- */
/**
 * This function checks the pipe from which we receive data about the current working directory.
 * If there is any data waiting, we clear it.
 */

static void
clear_cwd_pipe (void)
{
    fd_set read_set;
    struct timeval wtime = { 0, 0 };

    FD_ZERO (&read_set);
    FD_SET (subshell_pipe[READ], &read_set);

    const int maxfdp = subshell_pipe[READ];

    if (select (maxfdp + 1, &read_set, NULL, NULL, &wtime) > 0
        && FD_ISSET (subshell_pipe[READ], &read_set))
    {
        if (read (subshell_pipe[READ], subshell_cwd, sizeof (subshell_cwd)) <= 0)
        {
            tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
            fprintf (stderr, "read (subshell_pipe[READ]...): %s\r\n", unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        synchronize ();
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
do_subshell_chdir (const vfs_path_t *vpath, gboolean update_prompt)
{
    char *pcwd;

    pcwd = vfs_path_to_str_flags (subshell_get_cwd (), 0, VPF_RECODE);

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

    /* If we are using a shell that doesn't support persistent command buffer, we need to clear
     * the command prompt before we send the cd command. */
    if (!use_persistent_buffer)
    {
        write_all (mc_global.tty.subshell_pty, "\003", 1);
        subshell_state = RUNNING_COMMAND;
        if (mc_global.shell->type != SHELL_FISH && !feed_subshell (QUIETLY, TRUE))
        {
            subshell_state = ACTIVE;
            return;
        }
    }

    /* A quick and dirty fix for fish shell. For some reason, fish does not
     * execute all the commands sent to it from Midnight Commander :(
     * An example of such buggy behavior is presented in ticket #4521.
     * TODO: Find the real cause and fix it "the right way" */
    if (mc_global.shell->type == SHELL_FISH)
    {
        write_all (mc_global.tty.subshell_pty, "\n", 1);
        subshell_state = RUNNING_COMMAND;
        feed_subshell (QUIETLY, TRUE);
    }

    /* The initial space keeps this out of the command history (in bash
       because we set "HISTCONTROL=ignorespace") */
    write_all (mc_global.tty.subshell_pty, " cd ", 4);

    if (vpath == NULL)
        write_all (mc_global.tty.subshell_pty, "/", 1);
    else
    {
        const char *translate = vfs_translate_path (vfs_path_as_str (vpath));

        if (translate == NULL)
            write_all (mc_global.tty.subshell_pty, ".", 1);
        else
        {
            GString *temp;

            temp = subshell_name_quote (translate);
            write_all (mc_global.tty.subshell_pty, temp->str, temp->len);
            g_string_free (temp, TRUE);
        }
    }

    write_all (mc_global.tty.subshell_pty, "\n", 1);

    subshell_state = RUNNING_COMMAND;
    if (!feed_subshell (QUIETLY, TRUE))
    {
        subshell_state = ACTIVE;
        return;
    }

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

            cwd = vfs_path_to_str_flags (subshell_get_cwd (), 0, VPF_STRIP_PASSWORD);
            vfs_print_message (_ ("Warning: Cannot change to %s.\n"), cwd);
            g_free (cwd);
        }
    }

    // Really escape Zsh/Fish history
    if (mc_global.shell->type == SHELL_ZSH || mc_global.shell->type == SHELL_FISH)
    {
        /* Per Zsh documentation last command prefixed with space lingers in the internal history
         * until the next command is entered before it vanishes. To make it vanish right away,
         * type a space and press return.
         *
         * Fish shell now also provides the same behavior:
         * https://github.com/fish-shell/fish-shell/commit/9fdc4f903b8b421b18389a0f290d72cc88c128bb
         * */
        write_all (mc_global.tty.subshell_pty, " \n", 2);
        subshell_state = RUNNING_COMMAND;
        feed_subshell (QUIETLY, TRUE);
    }

    update_subshell_prompt = FALSE;

    g_free (pcwd);
    // Make sure that MC never stores the CWD in a silly format
    // like /usr////lib/../bin, or the strcmp() above will fail
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
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
    // This must be remembered across calls to init_subshell()
    static char pty_name[BUF_SMALL];

    // Take the current (hopefully pristine) tty mode and make
    // a raw mode based on it now, before we do anything else with it
    init_raw_mode ();

    if (mc_global.tty.subshell_pty == 0)
    {  // First time through
        if (mc_global.shell->type == SHELL_NONE)
            return;

        // Create a pipe for receiving the subshell's CWD

        if (mc_global.shell->type == SHELL_TCSH)
        {
            g_snprintf (tcsh_fifo, sizeof (tcsh_fifo), "%s/mc.pipe.%d", mc_tmpdir (),
                        (int) getpid ());
            if (mkfifo (tcsh_fifo, 0600) == -1)
            {
                fprintf (stderr, "mkfifo(%s) failed: %s\r\n", tcsh_fifo, unix_error_string (errno));
                mc_global.tty.use_subshell = FALSE;
                return;
            }

            // Opening the FIFO as O_RDONLY or O_WRONLY causes deadlock

            if ((subshell_pipe[READ] = open (tcsh_fifo, O_RDWR)) == -1
                || (subshell_pipe[WRITE] = open (tcsh_fifo, O_RDWR)) == -1)
            {
                fprintf (stderr, _ ("Cannot open named pipe %s\n"), tcsh_fifo);
                perror (__FILE__ ": open");
                mc_global.tty.use_subshell = FALSE;
                return;
            }
        }
        else if (pipe (subshell_pipe) != 0)  // subshell_type is BASH, ASH_BUSYBOX, DASH or ZSH
        {
            perror (__FILE__ ": couldn't create pipe");
            mc_global.tty.use_subshell = FALSE;
            return;
        }

        if (mc_global.mc_run_mode == MC_RUN_FULL
            && (mc_global.shell->type == SHELL_BASH || mc_global.shell->type == SHELL_ZSH
                || mc_global.shell->type == SHELL_FISH))
            use_persistent_buffer = TRUE;

        if (use_persistent_buffer && pipe (command_buffer_pipe) != 0)
        {
            perror (__FILE__ ": couldn't create pipe");
            mc_global.tty.use_subshell = FALSE;
            return;
        }

        // Open a pty for talking to the subshell; do this after
        // acquiring the pipes, so the latter have a higher chance
        // of getting a number lower than 10 so the "pwd >&%d" in
        // the mc-injected precmd does not cause a syntax error, a
        // ten-second sleep at startup, and other problems.

        // FIXME: We may need to open a fresh pty each time on SVR4

#ifdef HAVE_OPENPTY
        if (openpty (&mc_global.tty.subshell_pty, &subshell_pty_slave, NULL, NULL, NULL))
        {
            fprintf (stderr, "Cannot open master and slave sides of pty: %s\n",
                     unix_error_string (errno));
            mc_global.tty.use_subshell = FALSE;
            return;
        }
#else
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
            fprintf (stderr, "Cannot open slave side of pty %s: %s\r\n", pty_name,
                     unix_error_string (errno));
            mc_global.tty.use_subshell = FALSE;
            return;
        }
#endif
    }

    // Fork the subshell

    subshell_alive = TRUE;
    subshell_stopped = FALSE;
    subshell_pid = my_fork ();

    if (subshell_pid == -1)
    {
        fprintf (stderr, "Cannot spawn the subshell process: %s\r\n", unix_error_string (errno));
        // We exit here because, if the process table is full, the
        // other method of running user commands won't work either
        exit (EXIT_FAILURE);
    }

    if (subshell_pid == 0)
    {
        // We are in the child process
        init_subshell_child (pty_name);
    }

    {
        gchar *precmd;

        precmd = init_subshell_precmd ();
        write_all (mc_global.tty.subshell_pty, precmd, strlen (precmd));
        g_free (precmd);
    }

    // Wait until the subshell has started up and processed the command
    subshell_state = RUNNING_COMMAND;
    tty_enable_interrupt_key ();
    if (!feed_subshell (QUIETLY, TRUE))
        mc_global.tty.use_subshell = FALSE;
    tty_disable_interrupt_key ();
    if (!subshell_alive)
        mc_global.tty.use_subshell = FALSE;  // Subshell died instantly, so don't use it

    /* Try out the persistent command buffer feature. If it doesn't work the first time, we
     * assume there must be something wrong with the shell, and we turn persistent buffer off
     * for good. This will save the user the trouble of having to wait for the persistent
     * buffer function to time out every time they try to close the subshell. */
    if (use_persistent_buffer && !read_command_line_buffer (TRUE))
        use_persistent_buffer = FALSE;
}

/* --------------------------------------------------------------------------------------------- */

int
invoke_subshell (const char *command, int how, vfs_path_t **new_dir_vpath)
{
    // Make the MC terminal transparent
    tcsetattr (STDOUT_FILENO, TCSANOW, &raw_mode);

    // Make the subshell change to MC's working directory
    if (new_dir_vpath != NULL)
        do_subshell_chdir (subshell_get_cwd (), TRUE);

    if (command == NULL)  // The user has done "C-o" from MC
    {
        if (subshell_state == INACTIVE)
        {
            subshell_state = ACTIVE;

            // FIXME: possibly take out this hack; the user can re-play it by hitting C-hyphen a few
            // times!
            if (subshell_ready && mc_global.mc_run_mode == MC_RUN_FULL)
                write_all (mc_global.tty.subshell_pty, " \b", 2);  // Hack to make prompt reappear

            if (use_persistent_buffer)
            {
                const char *s = input_get_ctext (cmdline);

                /* Check to make sure there are no non text characters in the command buffer,
                 * such as tab, or newline, as this could cause problems. */
                for (size_t i = 0; i < cmdline->buffer->len; i++)
                    if ((unsigned char) s[i] < 32 || (unsigned char) s[i] == 127)
                        g_string_overwrite_len (cmdline->buffer, i, " ", 1);

                // Write the command buffer to the subshell.
                write_all (mc_global.tty.subshell_pty, s, cmdline->buffer->len);

                // Put the cursor in the correct place in the subshell.
                const int pos = (int) (str_length (s) - cmdline->point);

                for (int i = 0; i < pos; i++)
                    write_all (mc_global.tty.subshell_pty, ESC_STR "[D", 3);
            }
        }
    }
    else  // MC has passed us a user command
    {
        // Before we write to the command prompt, we need to clear whatever
        // data is there, but only if we are using one of the shells that
        // doesn't support keeping command buffer contents, OR if there was
        // some sort of error.
        if (use_persistent_buffer)
            clear_cwd_pipe ();
        else
        {
            /* We don't need to call feed_subshell here if we are using fish, because of a
             * quirk in the behavior of that particular shell. */
            if (mc_global.shell->type != SHELL_FISH)
            {
                write_all (mc_global.tty.subshell_pty, "\003", 1);
                subshell_state = RUNNING_COMMAND;
                feed_subshell (QUIETLY, FALSE);
            }
        }

        if (how == QUIETLY)
            write_all (mc_global.tty.subshell_pty, " ", 1);
        // FIXME: if command is long (>8KB ?) we go comma
        write_all (mc_global.tty.subshell_pty, command, strlen (command));
        write_all (mc_global.tty.subshell_pty, "\n", 1);
        subshell_state = RUNNING_COMMAND;
        subshell_ready = FALSE;
    }

    feed_subshell (how, FALSE);

    if (new_dir_vpath != NULL && subshell_alive)
    {
        const char *pcwd = vfs_translate_path (vfs_path_as_str (subshell_get_cwd ()));

        if (strcmp (subshell_cwd, pcwd) != 0)
            *new_dir_vpath =
                vfs_path_from_str (subshell_cwd);  // Make MC change to the subshell's CWD
    }

    // Restart the subshell if it has died by SIGHUP, SIGQUIT, etc.
    while (!subshell_alive && subshell_get_mainloop_quit () == 0 && mc_global.tty.use_subshell)
        init_subshell ();

    return subshell_get_mainloop_quit ();
}

/* --------------------------------------------------------------------------------------------- */

gboolean
flush_subshell (int max_wait_length, int how)
{
    int rc = 0;
    struct timeval timeleft = {
        .tv_sec = max_wait_length,
        .tv_usec = 0,
    };
    gboolean return_value = FALSE;
    fd_set tmp;

    FD_ZERO (&tmp);
    FD_SET (mc_global.tty.subshell_pty, &tmp);

    while (subshell_alive
           && (rc = select (mc_global.tty.subshell_pty + 1, &tmp, NULL, NULL, &timeleft)) != 0)
    {
        // Check for 'select' errors
        if (rc == -1)
        {
            if (errno == EINTR)
            {
                if (tty_got_winch ())
                    tty_change_screen_size ();

                continue;
            }

            fprintf (stderr, "select (FD_SETSIZE, &tmp...): %s\r\n", unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        return_value = TRUE;
        timeleft.tv_sec = 0;
        timeleft.tv_usec = 0;

        const ssize_t bytes = read (mc_global.tty.subshell_pty, pty_buffer, sizeof (pty_buffer));

        // FIXME: what about bytes <= 0?
        if (how == VISIBLY)
            write_all (STDOUT_FILENO, pty_buffer, (size_t) bytes);
    }

    return return_value;
}

/* --------------------------------------------------------------------------------------------- */

gboolean
read_subshell_prompt (void)
{
    int rc = 0;
    ssize_t bytes = 0;
    struct timeval timeleft = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    gboolean got_new_prompt = FALSE;
    fd_set tmp;

    FD_ZERO (&tmp);
    FD_SET (mc_global.tty.subshell_pty, &tmp);

    while (subshell_alive
           && (rc = select (mc_global.tty.subshell_pty + 1, &tmp, NULL, NULL, &timeleft)) != 0)
    {
        // Check for 'select' errors
        if (rc == -1)
        {
            if (errno == EINTR)
            {
                if (tty_got_winch ())
                    tty_change_screen_size ();

                continue;
            }

            fprintf (stderr, "select (FD_SETSIZE, &tmp...): %s\r\n", unix_error_string (errno));
            exit (EXIT_FAILURE);
        }

        bytes = read (mc_global.tty.subshell_pty, pty_buffer, sizeof (pty_buffer));

        parse_subshell_prompt_string (pty_buffer, bytes);
        got_new_prompt = TRUE;
    }

    if (got_new_prompt)
        set_prompt_string ();

    return (rc != 0 || bytes != 0);
}

/* --------------------------------------------------------------------------------------------- */

void
do_update_prompt (void)
{
    if (update_subshell_prompt)
    {
        if (subshell_prompt != NULL)
        {
            printf ("\r\n%s", subshell_prompt->str);
            fflush (stdout);
        }
        update_subshell_prompt = FALSE;
    }
}

/* --------------------------------------------------------------------------------------------- */

gboolean
exit_subshell (void)
{
    gboolean subshell_quit = TRUE;

    if (subshell_state != INACTIVE && subshell_alive)
        subshell_quit = query_dialog (_ ("Warning"), _ ("The shell is still active. Quit anyway?"),
                                      D_NORMAL, 2, _ ("&Yes"), _ ("&No"))
            == 0;

    if (subshell_quit)
    {
        if (mc_global.shell->type == SHELL_TCSH)
        {
            if (unlink (tcsh_fifo) == -1)
                fprintf (stderr, "Cannot remove named pipe %s: %s\r\n", tcsh_fifo,
                         unix_error_string (errno));
        }

        if (subshell_prompt != NULL)
        {
            g_string_free (subshell_prompt, TRUE);
            subshell_prompt = NULL;
        }

        if (subshell_prompt_temp_buffer != NULL)
        {
            g_string_free (subshell_prompt_temp_buffer, TRUE);
            subshell_prompt_temp_buffer = NULL;
        }

        pty_buffer[0] = '\0';
    }

    return subshell_quit;
}

/* --------------------------------------------------------------------------------------------- */

void
subshell_chdir (const vfs_path_t *vpath)
{
    if (mc_global.tty.use_subshell && vfs_current_is_local ())
        do_subshell_chdir (vpath, FALSE);
}

/* --------------------------------------------------------------------------------------------- */

void
subshell_get_console_attributes (void)
{
    // Get our current terminal modes

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
sigchld_handler (MC_UNUSED int sig)
{
    int status;
    const pid_t pid = waitpid (subshell_pid, &status, WUNTRACED | WNOHANG);

    if (pid == subshell_pid)
    {
        // Figure out what has happened to the subshell

        if (WIFSTOPPED (status))
        {
            if (WSTOPSIG (status) == SIGSTOP)
            {
                // The subshell has received a SIGSTOP signal
                subshell_stopped = TRUE;
            }
            else
            {
                // The user has suspended the subshell.  Revive it
                kill (subshell_pid, SIGCONT);
            }
        }
        else
        {
            // The subshell has either exited normally or been killed
            subshell_alive = FALSE;
            delete_select_channel (mc_global.tty.subshell_pty);
            if (WIFEXITED (status) && WEXITSTATUS (status) != FORK_FAILURE)
            {
                const int subshell_quit =
                    subshell_get_mainloop_quit () | SUBSHELL_EXIT;  // Exited normally

                subshell_set_mainloop_quit (subshell_quit);
            }
        }
    }

    subshell_handle_cons_saver ();

    // If we got here, some other child exited; ignore it
}

/* --------------------------------------------------------------------------------------------- */
