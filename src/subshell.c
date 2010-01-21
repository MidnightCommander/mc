/* Concurrent shell support for the Midnight Commander
   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of Version 2 of the GNU General Public
   License, as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file subshell.c
 *  \brief Source: concurrent shell support
 */

#include <config.h>

#ifdef HAVE_SUBSHELL_SUPPORT

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE 1
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#include <termios.h>
#include <unistd.h>

#ifdef HAVE_STROPTS_H
#  include <stropts.h> /* For I_PUSH */
#endif /* HAVE_STROPTS_H */

#include "lib/global.h"
#include "lib/tty/tty.h"	/* LINES */
#include "panel.h"	/* current_panel */
#include "wtools.h"	/* query_dialog() */
#include "main.h"	/* do_update_prompt() */
#include "cons.saver.h"	/* handle_console() */
#include "lib/tty/key.h"	/* XCTRL */
#include "subshell.h"
#include "strutil.h"

#include "lib/vfs/mc-vfs/vfs.h"

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif

#ifndef WIFEXITED
#   define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

/* tcsh closes all non-standard file descriptors, so we have to use a pipe */
static char tcsh_fifo[128];

/* Local functions */
static void init_raw_mode (void);
static int feed_subshell (int how, int fail_on_error);
static void synchronize (void);
static int pty_open_master (char *pty_name);
static int pty_open_slave (const char *pty_name);
static int resize_tty (int fd);

#ifndef STDIN_FILENO
#    define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#    define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#    define STDERR_FILENO 2
#endif

/* If using a subshell for evaluating commands this is true */
int use_subshell =
#ifdef SUBSHELL_OPTIONAL
FALSE;
#else
TRUE;
#endif

/* File descriptors of the pseudoterminal used by the subshell */
int subshell_pty = 0;
static int subshell_pty_slave = -1;

/* The key for switching back to MC from the subshell */
static const char subshell_switch_key = XCTRL('o') & 255;

/* State of the subshell:
 * INACTIVE: the default state; awaiting a command
 * ACTIVE: remain in the shell until the user hits `subshell_switch_key'
 * RUNNING_COMMAND: return to MC when the current command finishes */
enum subshell_state_enum subshell_state;

/* Holds the latest prompt captured from the subshell */
char *subshell_prompt = NULL;

/* Initial length of the buffer for the subshell's prompt */
#define INITIAL_PROMPT_SIZE 10

/* Used by the child process to indicate failure to start the subshell */
#define FORK_FAILURE 69  /* Arbitrary */

/* Initial length of the buffer for all I/O with the subshell */
#define INITIAL_PTY_BUFFER_SIZE 100  /* Arbitrary; but keep it >= 80 */

/* For pipes */
enum {READ=0, WRITE=1};

static char *pty_buffer;	/* For reading/writing on the subshell's pty */
static int pty_buffer_size;	/* The buffer grows as needed */
static int subshell_pipe[2];	/* To pass CWD info from the subshell to MC */
static pid_t subshell_pid = 1;	/* The subshell's process ID */
static char subshell_cwd[MC_MAXPATHLEN+1];  /* One extra char for final '\n' */

/* Subshell type (gleaned from the SHELL environment variable, if available) */
static enum {
    BASH,
    TCSH,
    ZSH,
    FISH
} subshell_type;

/* Flag to indicate whether the subshell is ready for next command */
static int subshell_ready;

/* The following two flags can be changed by the SIGCHLD handler. This is */
/* OK, because the `int' type is updated atomically on all known machines */
static volatile int subshell_alive, subshell_stopped;

/* We store the terminal's initial mode here so that we can configure
   the pty similarly, and also so we can restore the real terminal to
   sanity if we have to exit abruptly */
static struct termios shell_mode;

/* This is a transparent mode for the terminal where MC is running on */
/* It is used when the shell is active, so that the control signals */
/* are delivered to the shell pty */
static struct termios raw_mode;

/* This counter indicates how many characters of prompt we have read */
/* FIXME: try to figure out why this had to become global */
static int prompt_pos;


/*
 *  Write all data, even if the write() call is interrupted.
 */
static ssize_t
write_all (int fd, const void *buf, size_t count)
{
    ssize_t ret;
    ssize_t written = 0;
    while (count > 0) {
	ret = write (fd, (const unsigned char *) buf + written, count);
	if (ret < 0) {
	    if (errno == EINTR) {
		continue;
	    } else {
		return written > 0 ? written : ret;
	    }
	}
	count -= ret;
	written += ret;
    }
    return written;
}

/*
 *  Prepare child process to running the shell and run it.
 *
 *  Modifies the global variables (in the child process only):
 *	shell_mode
 *
 *  Returns: never.
 */
static void
init_subshell_child (const char *pty_name)
{
    const char *init_file = NULL;
    pid_t mc_sid;

    (void) pty_name;
    setsid ();			/* Get a fresh terminal session */

    /* Make sure that it has become our controlling terminal */

    /* Redundant on Linux and probably most systems, but just in case: */

#ifdef TIOCSCTTY
    ioctl (subshell_pty_slave, TIOCSCTTY, 0);
#endif

    /* Configure its terminal modes and window size */

    /* Set up the pty with the same termios flags as our own tty */
    if (tcsetattr (subshell_pty_slave, TCSANOW, &shell_mode)) {
	fprintf (stderr, "Cannot set pty terminal modes: %s\r\n",
		 unix_error_string (errno));
	_exit (FORK_FAILURE);
    }

    /* Set the pty's size (80x25 by default on Linux) according to the */
    /* size of the real terminal as calculated by ncurses, if possible */
    resize_tty (subshell_pty_slave);

    /* Set up the subshell's environment and init file name */

    /* It simplifies things to change to our home directory here, */
    /* and the user's startup file may do a `cd' command anyway   */
    chdir (home_dir);		/* FIXME? What about when we re-run the subshell? */

    /* Set MC_SID to prevent running one mc from another */
    mc_sid = getsid (0);
    if (mc_sid != -1) {
	char sid_str[BUF_SMALL];
	g_snprintf (sid_str, sizeof (sid_str), "MC_SID=%ld",
		    (long) mc_sid);
	putenv (g_strdup (sid_str));
    }

    switch (subshell_type) {
    case BASH:
	init_file = MC_USERCONF_DIR PATH_SEP_STR "bashrc";
	if (access (init_file, R_OK) == -1)
	    init_file = ".bashrc";

	/* Make MC's special commands not show up in bash's history */
	putenv ((char*)"HISTCONTROL=ignorespace");

	/* Allow alternative readline settings for MC */
	if (access (MC_USERCONF_DIR PATH_SEP_STR "inputrc", R_OK) == 0)
	    putenv ((char*)"INPUTRC=" MC_USERCONF_DIR PATH_SEP_STR "/inputrc");

	break;

	/* TODO: Find a way to pass initfile to TCSH and ZSH */
    case TCSH:
    case ZSH:
    case FISH:
	break;

    default:
	fprintf (stderr, __FILE__ ": unimplemented subshell type %d\r\n",
		 subshell_type);
	_exit (FORK_FAILURE);
    }

    /* Attach all our standard file descriptors to the pty */

    /* This is done just before the fork, because stderr must still      */
    /* be connected to the real tty during the above error messages; */
    /* otherwise the user will never see them.                   */

    dup2 (subshell_pty_slave, STDIN_FILENO);
    dup2 (subshell_pty_slave, STDOUT_FILENO);
    dup2 (subshell_pty_slave, STDERR_FILENO);

    close (subshell_pipe[READ]);
    close (subshell_pty_slave);		/* These may be FD_CLOEXEC, but just in case... */
    /* Close master side of pty.  This is important; apart from */
    /* freeing up the descriptor for use in the subshell, it also       */
    /* means that when MC exits, the subshell will get a SIGHUP and     */
    /* exit too, because there will be no more descriptors pointing     */
    /* at the master side of the pty and so it will disappear.  */
    close (subshell_pty);

    /* Execute the subshell at last */

    switch (subshell_type) {
    case BASH:
	execl (shell, "bash", "-rcfile", init_file, (char *) NULL);
	break;

    case TCSH:
	execl (shell, "tcsh", (char *) NULL);
	break;

    case ZSH:
	/* Use -g to exclude cmds beginning with space from history
	 * and -Z to use the line editor on non-interactive term */
	execl (shell, "zsh", "-Z", "-g", (char *) NULL);

	break;

    case FISH:
	execl (shell, "fish", (char *) NULL);
        break;
    }

    /* If we get this far, everything failed miserably */
    _exit (FORK_FAILURE);
}


/*
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
    if (!sid_str)
	return 0;

    old_sid = (pid_t) strtol (sid_str, NULL, 0);
    if (!old_sid)
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
			"Subshell support will be disabled."), D_ERROR, 2,
		      _("&OK"), _("&Quit"));
    if (r != 0) {
	return 2;
    }

    return 1;
}


/*
 *  Fork the subshell, and set up many, many things.
 *
 *  Possibly modifies the global variables:
 *	subshell_type, subshell_alive, subshell_stopped, subshell_pid
 *	use_subshell - Is set to FALSE if we can't run the subshell
 *	quit - Can be set to SUBSHELL_EXIT by the SIGCHLD handler
 */

void
init_subshell (void)
{
    /* This must be remembered across calls to init_subshell() */
    static char pty_name[BUF_SMALL];
    char precmd[BUF_SMALL];

    switch (check_sid ()) {
    case 1:
	use_subshell = FALSE;
	return;
    case 2:
	use_subshell = FALSE;
	midnight_shutdown = 1;
	return;
    }

    /* Take the current (hopefully pristine) tty mode and make */
    /* a raw mode based on it now, before we do anything else with it */
    init_raw_mode ();

    if (subshell_pty == 0) {	/* First time through */
	/* Find out what type of shell we have */

	if (strstr (shell, "/zsh") || getenv ("ZSH_VERSION"))
	    subshell_type = ZSH;
	else if (strstr (shell, "/tcsh"))
	    subshell_type = TCSH;
	else if (strstr (shell, "/csh"))
	    subshell_type = TCSH;
	else if (strstr (shell, "/bash") || getenv ("BASH"))
	    subshell_type = BASH;
	else if (strstr (shell, "/fish"))
	    subshell_type = FISH;
	else {
	    use_subshell = FALSE;
	    return;
	}

	/* Open a pty for talking to the subshell */

	/* FIXME: We may need to open a fresh pty each time on SVR4 */

	subshell_pty = pty_open_master (pty_name);
	if (subshell_pty == -1) {
	    fprintf (stderr, "Cannot open master side of pty: %s\r\n",
		     unix_error_string (errno));
	    use_subshell = FALSE;
	    return;
	}
	subshell_pty_slave = pty_open_slave (pty_name);
	if (subshell_pty_slave == -1) {
	    fprintf (stderr, "Cannot open slave side of pty %s: %s\r\n",
		     pty_name, unix_error_string (errno));
	    use_subshell = FALSE;
	    return;
	}

	/* Initialise the pty's I/O buffer */

	pty_buffer_size = INITIAL_PTY_BUFFER_SIZE;
	pty_buffer = g_malloc (pty_buffer_size);

	/* Create a pipe for receiving the subshell's CWD */

	if (subshell_type == TCSH) {
	    g_snprintf (tcsh_fifo, sizeof (tcsh_fifo), "%s/mc.pipe.%d",
			mc_tmpdir (), (int) getpid ());
	    if (mkfifo (tcsh_fifo, 0600) == -1) {
		fprintf (stderr, "mkfifo(%s) failed: %s\r\n", tcsh_fifo,
			 unix_error_string (errno));
		use_subshell = FALSE;
		return;
	    }

	    /* Opening the FIFO as O_RDONLY or O_WRONLY causes deadlock */

	    if ((subshell_pipe[READ] = open (tcsh_fifo, O_RDWR)) == -1
		|| (subshell_pipe[WRITE] =
		    open (tcsh_fifo, O_RDWR)) == -1) {
		fprintf (stderr, _("Cannot open named pipe %s\n"), tcsh_fifo);
		perror (__FILE__": open");
		use_subshell = FALSE;
		return;
	    }
	} else /* subshell_type is BASH or ZSH */ if (pipe (subshell_pipe)) {
	    perror (__FILE__": couldn't create pipe");
	    use_subshell = FALSE;
	    return;
	}
    }

    /* Fork the subshell */

    subshell_alive = TRUE;
    subshell_stopped = FALSE;
    subshell_pid = fork ();

    if (subshell_pid == -1) {
	fprintf (stderr, "Cannot spawn the subshell process: %s\r\n",
		 unix_error_string (errno));
	/* We exit here because, if the process table is full, the */
	/* other method of running user commands won't work either */
	exit (1);
    }

    if (subshell_pid == 0) {	/* We are in the child process */
	init_subshell_child (pty_name);
    }

    /* Set up `precmd' or equivalent for reading the subshell's CWD */

    switch (subshell_type) {
    case BASH:
	g_snprintf (precmd, sizeof (precmd),
		    " PROMPT_COMMAND='pwd>&%d;kill -STOP $$'\n",
		    subshell_pipe[WRITE]);
	break;

    case ZSH:
	g_snprintf (precmd, sizeof (precmd),
		    " precmd(){ pwd>&%d;kill -STOP $$ }\n",
		    subshell_pipe[WRITE]);
	break;

    case TCSH:
	g_snprintf (precmd, sizeof (precmd),
		    "set echo_style=both;"
		    "alias precmd 'echo $cwd:q >>%s;kill -STOP $$'\n",
		    tcsh_fifo);
	break;
    case FISH:
	g_snprintf (precmd, sizeof (precmd),
		    "function fish_prompt ; pwd>&%d;kill -STOP %%self; end\n",
		    subshell_pipe[WRITE]);
	break;

    }
    write_all (subshell_pty, precmd, strlen (precmd));

    /* Wait until the subshell has started up and processed the command */

    subshell_state = RUNNING_COMMAND;
    tty_enable_interrupt_key ();
    if (!feed_subshell (QUIETLY, TRUE)) {
	use_subshell = FALSE;
    }
    tty_disable_interrupt_key ();
    if (!subshell_alive)
	use_subshell = FALSE;	/* Subshell died instantly, so don't use it */
}


static void init_raw_mode ()
{
    static int initialized = 0;

    /* MC calls tty_reset_shell_mode() in pre_exec() to set the real tty to its */
    /* original settings.  However, here we need to make this tty very raw,     */
    /* so that all keyboard signals, XON/XOFF, etc. will get through to the     */
    /* pty.  So, instead of changing the code for execute(), pre_exec(),        */
    /* etc, we just set up the modes we need here, before each command.         */

    if (initialized == 0)  /* First time: initialise `raw_mode' */
    {
	tcgetattr (STDOUT_FILENO, &raw_mode);
	raw_mode.c_lflag &= ~ICANON;  /* Disable line-editing chars, etc.   */
	raw_mode.c_lflag &= ~ISIG;    /* Disable intr, quit & suspend chars */
	raw_mode.c_lflag &= ~ECHO;    /* Disable input echoing		    */
	raw_mode.c_iflag &= ~IXON;    /* Pass ^S/^Q to subshell undisturbed */
	raw_mode.c_iflag &= ~ICRNL;   /* Don't translate CRs into LFs	    */
	raw_mode.c_oflag &= ~OPOST;   /* Don't postprocess output	    */
	raw_mode.c_cc[VTIME] = 0;     /* IE: wait forever, and return as    */
	raw_mode.c_cc[VMIN] = 1;      /* soon as a character is available   */
	initialized = 1;
    }
}


int invoke_subshell (const char *command, int how, char **new_dir)
{
    char *pcwd;
    
    /* Make the MC terminal transparent */
    tcsetattr (STDOUT_FILENO, TCSANOW, &raw_mode);
    
    /* Make the subshell change to MC's working directory */
    if (new_dir)
	do_subshell_chdir (current_panel->cwd, TRUE, 1);
    
    if (command == NULL)  /* The user has done "C-o" from MC */
    {
	if (subshell_state == INACTIVE)
	{
	    subshell_state = ACTIVE;
	    /* FIXME: possibly take out this hack; the user can
	       re-play it by hitting C-hyphen a few times! */
	    if (subshell_ready)
		write_all (subshell_pty, " \b", 2);  /* Hack to make prompt reappear */
	}
    }
    else  /* MC has passed us a user command */
    {
	if (how == QUIETLY)
	    write_all (subshell_pty, " ", 1);
	/* FIXME: if command is long (>8KB ?) we go comma */
	write_all (subshell_pty, command, strlen (command));
	write_all (subshell_pty, "\n", 1);
	subshell_state = RUNNING_COMMAND;
	subshell_ready = FALSE;
    }

    feed_subshell (how, FALSE);

    pcwd = vfs_translate_path_n (current_panel->cwd);
    if (new_dir && subshell_alive && strcmp (subshell_cwd, pcwd))
	*new_dir = subshell_cwd;  /* Make MC change to the subshell's CWD */
    g_free (pcwd);

    /* Restart the subshell if it has died by SIGHUP, SIGQUIT, etc. */
    while (!subshell_alive && !quit && use_subshell)
	init_subshell ();

    prompt_pos = 0;

    return quit;
}


int
read_subshell_prompt (void)
{
    static int prompt_size = INITIAL_PROMPT_SIZE;
    int bytes = 0, i, rc = 0;
    struct timeval timeleft = { 0, 0 };

    fd_set tmp;
    FD_ZERO (&tmp);
    FD_SET (subshell_pty, &tmp);

    if (subshell_prompt == NULL) {	/* First time through */
	subshell_prompt = g_malloc (prompt_size);
	*subshell_prompt = '\0';
	prompt_pos = 0;
    }

    while (subshell_alive
	   && (rc =
	       select (subshell_pty + 1, &tmp, NULL, NULL, &timeleft))) {
	/* Check for `select' errors */
	if (rc == -1) {
	    if (errno == EINTR)
		continue;
	    else {
		fprintf (stderr, "select (FD_SETSIZE, &tmp...): %s\r\n",
			 unix_error_string (errno));
		exit (1);
	    }
	}

	bytes = read (subshell_pty, pty_buffer, pty_buffer_size);

	/* Extract the prompt from the shell output */

	for (i = 0; i < bytes; ++i)
	    if (pty_buffer[i] == '\n' || pty_buffer[i] == '\r') {
		prompt_pos = 0;
	    } else {
		if (!pty_buffer[i])
		    continue;

		subshell_prompt[prompt_pos++] = pty_buffer[i];
		if (prompt_pos == prompt_size)
		    subshell_prompt =
			g_realloc (subshell_prompt, prompt_size *= 2);
	    }

	subshell_prompt[prompt_pos] = '\0';
    }
    if (rc == 0 && bytes == 0)
	return FALSE;
    return TRUE;
}

/* Resize given terminal using TIOCSWINSZ, return ioctl() result */
static int resize_tty (int fd)
{
#if defined TIOCSWINSZ
    struct winsize tty_size;

    tty_size.ws_row = LINES;
    tty_size.ws_col = COLS;
    tty_size.ws_xpixel = tty_size.ws_ypixel = 0;

    return ioctl (fd, TIOCSWINSZ, &tty_size);
#else
    return 0;
#endif
}

/* Resize subshell_pty */
void resize_subshell (void)
{
    if (use_subshell == 0)
	return;

    resize_tty (subshell_pty);
}

int
exit_subshell (void)
{
    int subshell_quit = TRUE;

    if (subshell_state != INACTIVE && subshell_alive)
	subshell_quit =
	    !query_dialog (_("Warning"),
			   _(" The shell is still active. Quit anyway? "),
			   D_NORMAL, 2, _("&Yes"), _("&No"));

    if (subshell_quit) {
	if (subshell_type == TCSH) {
	    if (unlink (tcsh_fifo) == -1)
		fprintf (stderr, "Cannot remove named pipe %s: %s\r\n",
			 tcsh_fifo, unix_error_string (errno));
	}

	g_free (subshell_prompt);
	g_free (pty_buffer);
	subshell_prompt = NULL;
	pty_buffer = NULL;
    }

    return subshell_quit;
}


/*
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
static char *
subshell_name_quote (const char *s)
{
    char *ret, *d;
    const char *su, *n;
    const char *quote_cmd_start, *quote_cmd_end;
    int c;

    if (subshell_type == FISH) {
         quote_cmd_start = "(printf \"%b\" '";
         quote_cmd_end = "')";
    } else {
         quote_cmd_start = "\"`printf \"%b\" '";
         quote_cmd_end = "'`\"";
    }

    /* Factor 5 because we need \, 0 and 3 other digits per character. */
    d = ret = g_try_malloc (1 + (5 * strlen (s)) + (strlen(quote_cmd_start))
				+ (strlen(quote_cmd_end)));
    if (d == NULL)
	return NULL;

    /* Prevent interpreting leading `-' as a switch for `cd' */
    if (*s == '-') {
	*d++ = '.';
	*d++ = '/';
    }

    /* Copy the beginning of the command to the buffer */
    strcpy (d, quote_cmd_start);
    d += strlen(quote_cmd_start);

    /*
     * Print every character except digits and letters as a backslash-escape
     * sequence of the form \0nnn, where "nnn" is the numeric value of the
     * character converted to octal number.
     */
    su = s;
    for (; su[0] != '\0'; ) {
	n = str_cget_next_char_safe (su);
	if (str_isalnum (su)) {
	    memcpy (d, su, n - su);
	    d+= n - su;
	} else {
	    for (c = 0; c < n - su; c++) {
		sprintf (d, "\\0%03o", (unsigned char) su[c]);
		d += 5;
	    }
	}
	su = n;
    }

    strcpy (d, quote_cmd_end);

    return ret;
}


/* If it actually changed the directory it returns true */
void
do_subshell_chdir (const char *directory, int do_update, int reset_prompt)
{
    char *pcwd;
    char *temp;
    char *translate;
    
    pcwd = vfs_translate_path_n (current_panel->cwd);
    
    if (!
	(subshell_state == INACTIVE
	 && strcmp (subshell_cwd, pcwd))) {
	/* We have to repaint the subshell prompt if we read it from
	 * the main program.  Please note that in the code after this
	 * if, the cd command that is sent will make the subshell
	 * repaint the prompt, so we don't have to paint it. */
	if (do_update)
	    do_update_prompt ();
        g_free (pcwd);
	return;
    }

    /* The initial space keeps this out of the command history (in bash
       because we set "HISTCONTROL=ignorespace") */
    write_all (subshell_pty, " cd ", 4);
    if (*directory) {
        translate = vfs_translate_path_n (directory);
        if (translate) {
            temp = subshell_name_quote (translate);
	if (temp) {
	    write_all (subshell_pty, temp, strlen (temp));
	    g_free (temp);
	} else {
	    /* Should not happen unless the directory name is so long
	       that we don't have memory to quote it.  */
	    write_all (subshell_pty, ".", 1);
	}
            g_free (translate);
        } else {
            write_all (subshell_pty, ".", 1);
        }
    } else {
	write_all (subshell_pty, "/", 1);
    }
    write_all (subshell_pty, "\n", 1);

    subshell_state = RUNNING_COMMAND;
    feed_subshell (QUIETLY, FALSE);

    if (subshell_alive) {
        int bPathNotEq = strcmp (subshell_cwd, pcwd);

	if (bPathNotEq && subshell_type == TCSH) {
	    char rp_subshell_cwd[PATH_MAX];
	    char rp_current_panel_cwd[PATH_MAX];

	    char *p_subshell_cwd =
		mc_realpath (subshell_cwd, rp_subshell_cwd);
	    char *p_current_panel_cwd =
                    mc_realpath (pcwd, rp_current_panel_cwd);

	    if (p_subshell_cwd == NULL)
		p_subshell_cwd = subshell_cwd;
	    if (p_current_panel_cwd == NULL)
                p_current_panel_cwd = pcwd;
	    bPathNotEq = strcmp (p_subshell_cwd, p_current_panel_cwd);
	}

        if (bPathNotEq && strcmp (pcwd, ".")) {
            char *cwd = strip_password (g_strdup (pcwd), 1);
	    fprintf (stderr, _("Warning: Cannot change to %s.\n"), cwd);
	    g_free (cwd);
	}
    }

    if (reset_prompt)
	prompt_pos = 0;
    update_prompt = FALSE;
    
    g_free (pcwd);
    /* Make sure that MC never stores the CWD in a silly format */
    /* like /usr////lib/../bin, or the strcmp() above will fail */
}


void
subshell_get_console_attributes (void)
{
    /* Get our current terminal modes */

    if (tcgetattr (STDOUT_FILENO, &shell_mode)) {
	fprintf (stderr, "Cannot get terminal settings: %s\r\n",
		 unix_error_string (errno));
	use_subshell = FALSE;
	return;
    }
}


/* Figure out whether the subshell has stopped, exited or been killed */
/* Possibly modifies: `subshell_alive', `subshell_stopped' and `quit' */
void
sigchld_handler (int sig)
{
    int status;
    pid_t pid;

    (void) sig;

    pid = waitpid (subshell_pid, &status, WUNTRACED | WNOHANG);

    if (pid == subshell_pid) {
	/* Figure out what has happened to the subshell */

	if (WIFSTOPPED (status)) {
	    if (WSTOPSIG (status) == SIGSTOP) {
		/* The subshell has received a SIGSTOP signal */
		subshell_stopped = TRUE;
	    } else {
		/* The user has suspended the subshell.  Revive it */
		kill (subshell_pid, SIGCONT);
	    }
	} else {
	    /* The subshell has either exited normally or been killed */
	    subshell_alive = FALSE;
	    delete_select_channel (subshell_pty);
	    if (WIFEXITED (status) && WEXITSTATUS (status) != FORK_FAILURE)
		quit |= SUBSHELL_EXIT;	/* Exited normally */
	}
    }
#ifdef __linux__
    pid = waitpid (cons_saver_pid, &status, WUNTRACED | WNOHANG);

    if (pid == cons_saver_pid) {

	if (WIFSTOPPED (status))
	    /* Someone has stopped cons.saver - restart it */
	    kill (pid, SIGCONT);
	else {
	    /* cons.saver has died - disable confole saving */
	    handle_console (CONSOLE_DONE);
	    console_flag = 0;
	}

    }
#endif				/* __linux__ */

    /* If we got here, some other child exited; ignore it */
}


/* Feed the subshell our keyboard input until it says it's finished */
static int
feed_subshell (int how, int fail_on_error)
{
    fd_set read_set;		/* For `select' */
    int maxfdp;
    int bytes;			/* For the return value from `read' */
    int i;			/* Loop counter */

    struct timeval wtime;	/* Maximum time we wait for the subshell */
    struct timeval *wptr;

    /* we wait up to 10 seconds if fail_on_error, forever otherwise */
    wtime.tv_sec = 10;
    wtime.tv_usec = 0;
    wptr = fail_on_error ? &wtime : NULL;

    while (1) {
	if (!subshell_alive)
	    return FALSE;

	/* Prepare the file-descriptor set and call `select' */

	FD_ZERO (&read_set);
	FD_SET (subshell_pty, &read_set);
	FD_SET (subshell_pipe[READ], &read_set);
	maxfdp = max (subshell_pty, subshell_pipe[READ]);
	if (how == VISIBLY) {
	    FD_SET (STDIN_FILENO, &read_set);
	    maxfdp = max (maxfdp, STDIN_FILENO);
	}

	if (select (maxfdp + 1, &read_set, NULL, NULL, wptr) == -1) {

	    /* Despite using SA_RESTART, we still have to check for this */
	    if (errno == EINTR)
		continue;	/* try all over again */
	    tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
	    fprintf (stderr, "select (FD_SETSIZE, &read_set...): %s\r\n",
		     unix_error_string (errno));
	    exit (1);
	}

	if (FD_ISSET (subshell_pty, &read_set))
	    /* Read from the subshell, write to stdout */

	    /* This loop improves performance by reducing context switches
	       by a factor of 20 or so... unfortunately, it also hangs MC
	       randomly, because of an apparent Linux bug.  Investigate. */
	    /* for (i=0; i<5; ++i)  * FIXME -- experimental */
	{
	    bytes = read (subshell_pty, pty_buffer, pty_buffer_size);

	    /* The subshell has died */
	    if (bytes == -1 && errno == EIO && !subshell_alive)
		return FALSE;

	    if (bytes <= 0) {
		tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
		fprintf (stderr, "read (subshell_pty...): %s\r\n",
			 unix_error_string (errno));
		exit (1);
	    }

	    if (how == VISIBLY)
		write_all (STDOUT_FILENO, pty_buffer, bytes);
	}

	else if (FD_ISSET (subshell_pipe[READ], &read_set))
	    /* Read the subshell's CWD and capture its prompt */

	{
	    bytes =
		read (subshell_pipe[READ], subshell_cwd,
		      MC_MAXPATHLEN + 1);
	    if (bytes <= 0) {
		tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
		fprintf (stderr, "read (subshell_pipe[READ]...): %s\r\n",
			 unix_error_string (errno));
		exit (1);
	    }

	    subshell_cwd[bytes - 1] = 0;	/* Squash the final '\n' */

	    synchronize ();

	    subshell_ready = TRUE;
	    if (subshell_state == RUNNING_COMMAND) {
		subshell_state = INACTIVE;
		return 1;
	    }
	}

	else if (FD_ISSET (STDIN_FILENO, &read_set))
	    /* Read from stdin, write to the subshell */
	{
	    bytes = read (STDIN_FILENO, pty_buffer, pty_buffer_size);
	    if (bytes <= 0) {
		tcsetattr (STDOUT_FILENO, TCSANOW, &shell_mode);
		fprintf (stderr,
			 "read (STDIN_FILENO, pty_buffer...): %s\r\n",
			 unix_error_string (errno));
		exit (1);
	    }

	    for (i = 0; i < bytes; ++i)
		if (pty_buffer[i] == subshell_switch_key) {
		    write_all (subshell_pty, pty_buffer, i);
		    if (subshell_ready)
			subshell_state = INACTIVE;
		    return TRUE;
		}

	    write_all (subshell_pty, pty_buffer, bytes);

	    if (pty_buffer[bytes-1] == '\n' || pty_buffer[bytes-1] == '\r')
		subshell_ready = FALSE;
	} else {
	    return FALSE;
	}
    }
}


/* Wait until the subshell dies or stops.  If it stops, make it resume.  */
/* Possibly modifies the globals `subshell_alive' and `subshell_stopped' */
static void synchronize (void)
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

    if (subshell_state != ACTIVE) {
	/* Discard all remaining data from stdin to the subshell */
	tcflush (subshell_pty_slave, TCIFLUSH);
    }

    subshell_stopped = FALSE;
    kill (subshell_pid, SIGCONT);

    sigprocmask (SIG_SETMASK, &old_mask, NULL);
    /* We can't do any better without modifying the shell(s) */
}

/* pty opening functions */

#ifdef HAVE_GRANTPT

/* System V version of pty_open_master */

static int pty_open_master (char *pty_name)
{
    char *slave_name;
    int pty_master;

#ifdef HAVE_POSIX_OPENPT
    pty_master = posix_openpt(O_RDWR);
#elif HAVE_GETPT
    /* getpt () is a GNU extension (glibc 2.1.x) */
    pty_master = getpt ();
#elif IS_AIX
    strcpy (pty_name, "/dev/ptc");
    pty_master = open (pty_name, O_RDWR);
#else
    strcpy (pty_name, "/dev/ptmx");
    pty_master = open (pty_name, O_RDWR);
#endif 

    if (pty_master == -1)
	return -1;

    if (grantpt (pty_master) == -1		  /* Grant access to slave */
	|| unlockpt (pty_master) == -1		  /* Clear slave's lock flag */
	|| !(slave_name = ptsname (pty_master)))  /* Get slave's name */
    {
	close (pty_master);
	return -1;
    }
    strcpy (pty_name, slave_name);
    return pty_master;
}

/* System V version of pty_open_slave */
static int
pty_open_slave (const char *pty_name)
{
    int pty_slave = open (pty_name, O_RDWR);

    if (pty_slave == -1) {
	fprintf (stderr, "open (%s, O_RDWR): %s\r\n", pty_name,
		 unix_error_string (errno));
	return -1;
    }
#if !defined(__osf__) && !defined(__linux__)
#if defined (I_FIND) && defined (I_PUSH)
    if (!ioctl (pty_slave, I_FIND, "ptem"))
	if (ioctl (pty_slave, I_PUSH, "ptem") == -1) {
	    fprintf (stderr, "ioctl (%d, I_PUSH, \"ptem\") failed: %s\r\n",
		     pty_slave, unix_error_string (errno));
	    close (pty_slave);
	    return -1;
	}

    if (!ioctl (pty_slave, I_FIND, "ldterm"))
	if (ioctl (pty_slave, I_PUSH, "ldterm") == -1) {
	    fprintf (stderr,
		     "ioctl (%d, I_PUSH, \"ldterm\") failed: %s\r\n",
		     pty_slave, unix_error_string (errno));
	    close (pty_slave);
	    return -1;
	}
#if !defined(sgi) && !defined(__sgi)
    if (!ioctl (pty_slave, I_FIND, "ttcompat"))
	if (ioctl (pty_slave, I_PUSH, "ttcompat") == -1) {
	    fprintf (stderr,
		     "ioctl (%d, I_PUSH, \"ttcompat\") failed: %s\r\n",
		     pty_slave, unix_error_string (errno));
	    close (pty_slave);
	    return -1;
	}
#endif				/* sgi || __sgi */
#endif				/* I_FIND && I_PUSH */
#endif				/* __osf__ || __linux__ */

    fcntl(pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}

#else /* !HAVE_GRANTPT */

/* BSD version of pty_open_master */
static int pty_open_master (char *pty_name)
{
    int pty_master;
    const char *ptr1, *ptr2;

    strcpy (pty_name, "/dev/ptyXX");
    for (ptr1 = "pqrstuvwxyzPQRST"; *ptr1; ++ptr1)
    {
	pty_name [8] = *ptr1;
	for (ptr2 = "0123456789abcdef"; *ptr2; ++ptr2)
	{
	    pty_name [9] = *ptr2;

	    /* Try to open master */
	    if ((pty_master = open (pty_name, O_RDWR)) == -1) {
		if (errno == ENOENT)  /* Different from EIO */
		    return -1;	      /* Out of pty devices */
		else
		    continue;	      /* Try next pty device */
	    }
	    pty_name [5] = 't';	      /* Change "pty" to "tty" */
	    if (access (pty_name, 6)){
		close (pty_master);
		pty_name [5] = 'p';
		continue;
	    }
	    return pty_master;
	}
    }
    return -1;  /* Ran out of pty devices */
}

/* BSD version of pty_open_slave */
static int
pty_open_slave (const char *pty_name)
{
    int pty_slave;
    struct group *group_info = getgrnam ("tty");

    if (group_info != NULL) {
	/* The following two calls will only succeed if we are root */
	/* [Commented out while permissions problem is investigated] */
	/* chown (pty_name, getuid (), group_info->gr_gid);  FIXME */
	/* chmod (pty_name, S_IRUSR | S_IWUSR | S_IWGRP);   FIXME */
    }
    if ((pty_slave = open (pty_name, O_RDWR)) == -1)
	fprintf (stderr, "open (pty_name, O_RDWR): %s\r\n", pty_name);
    fcntl(pty_slave, F_SETFD, FD_CLOEXEC);
    return pty_slave;
}

#endif /* !HAVE_GRANTPT */
#endif /* HAVE_SUBSHELL_SUPPORT */
