/*
   lib - common code for testing lib/utilinux:my_system() function

   Copyright (C) 2013
   The Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

#include <signal.h>
#include <unistd.h>

#include "lib/vfs/vfs.h"

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static sigset_t *sigemptyset_set__captured;
/* @ThenReturnValue */
static int sigemptyset__return_value = 0;

/* @Mock */
int
sigemptyset (sigset_t * set)
{
    sigemptyset_set__captured = set;
    return sigemptyset__return_value;
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GPtrArray *sigaction_signum__captured = NULL;
/* @CapturedValue */
static GPtrArray *sigaction_act__captured = NULL;
/* @CapturedValue */
static GPtrArray *sigaction_oldact__captured = NULL;
/* @ThenReturnValue */
static int sigaction__return_value = 0;

/* @Mock */
int
sigaction (int signum, const struct sigaction *act, struct sigaction *oldact)
{
    int *tmp_signum;
    struct sigaction *tmp_act;

    /* store signum */
    tmp_signum = g_new (int, 1);
    memcpy (tmp_signum, &signum, sizeof (int));
    if (sigaction_signum__captured != NULL)
        g_ptr_array_add (sigaction_signum__captured, tmp_signum);

    /* store act */
    if (act != NULL)
    {
        tmp_act = g_new (struct sigaction, 1);
        memcpy (tmp_act, act, sizeof (struct sigaction));
    }
    else
        tmp_act = NULL;
    if (sigaction_act__captured != NULL)
        g_ptr_array_add (sigaction_act__captured, tmp_act);

    /* store oldact */
    if (oldact != NULL)
    {
        tmp_act = g_new (struct sigaction, 1);
        memcpy (tmp_act, oldact, sizeof (struct sigaction));
    }
    else
        tmp_act = NULL;
    if (sigaction_oldact__captured != NULL)
        g_ptr_array_add (sigaction_oldact__captured, tmp_act);

    return sigaction__return_value;
}

static void
sigaction__init (void)
{
    sigaction_signum__captured = g_ptr_array_new ();
    sigaction_act__captured = g_ptr_array_new ();
    sigaction_oldact__captured = g_ptr_array_new ();
}

static void
sigaction__deinit (void)
{
    g_ptr_array_foreach (sigaction_signum__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (sigaction_signum__captured, TRUE);

    g_ptr_array_foreach (sigaction_act__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (sigaction_act__captured, TRUE);

    g_ptr_array_foreach (sigaction_oldact__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (sigaction_oldact__captured, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static GPtrArray *signal_signum__captured;
/* @CapturedValue */
static GPtrArray *signal_handler__captured;
/* @ThenReturnValue */
static sighandler_t signal__return_value = NULL;

/* @Mock */
sighandler_t
signal (int signum, sighandler_t handler)
{
    int *tmp_signum;
    sighandler_t *tmp_handler;

    /* store signum */
    tmp_signum = g_new (int, 1);
    memcpy (tmp_signum, &signum, sizeof (int));
    g_ptr_array_add (signal_signum__captured, tmp_signum);

    /* store handler */
    if (handler != SIG_DFL)
    {
        tmp_handler = g_new (sighandler_t, 1);
        memcpy (tmp_handler, handler, sizeof (sighandler_t));
    }
    else
        tmp_handler = (void *) SIG_DFL;
    g_ptr_array_add (signal_handler__captured, tmp_handler);

    return signal__return_value;
}

static void
signal__init (void)
{
    signal_signum__captured = g_ptr_array_new ();
    signal_handler__captured = g_ptr_array_new ();
}

static void
signal__deinit (void)
{
    g_ptr_array_foreach (signal_signum__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (signal_signum__captured, TRUE);

    g_ptr_array_foreach (signal_handler__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (signal_handler__captured, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

/* @ThenReturnValue */
static pid_t fork__return_value;

/* @Mock */
pid_t
fork (void)
{
    return fork__return_value;
}

/* --------------------------------------------------------------------------------------------- */
/* @CapturedValue */
static int my_exit__status__captured;

/* @Mock */
void
my_exit (int status)
{
    my_exit__status__captured = status;
}

/* --------------------------------------------------------------------------------------------- */

/* @CapturedValue */
static char *execvp__file__captured = NULL;
/* @CapturedValue */
static GPtrArray *execvp__args__captured;
/* @ThenReturnValue */
static int execvp__return_value = 0;

/* @Mock */
int
execvp (const char *file, char *const argv[])
{
    char **one_arg;
    execvp__file__captured = g_strdup (file);

    for (one_arg = (char **) argv; *one_arg != NULL; one_arg++)
        g_ptr_array_add (execvp__args__captured, g_strdup (*one_arg));

    return execvp__return_value;
}

static void
execvp__init (void)
{
    execvp__args__captured = g_ptr_array_new ();
}

static void
execvp__deinit (void)
{
    g_ptr_array_foreach (execvp__args__captured, (GFunc) g_free, NULL);
    g_ptr_array_free (execvp__args__captured, TRUE);
    g_free (execvp__file__captured);
}

/* --------------------------------------------------------------------------------------------- */

#define VERIFY_SIGACTION__ACT_IGNORED(_pntr) { \
    struct sigaction *_act = (struct sigaction *) _pntr; \
    mctest_assert_ptr_eq (_act->sa_handler, SIG_IGN); \
    mctest_assert_int_eq (_act->sa_flags, 0); \
}

#define VERIFY_SIGACTION__IS_RESTORED(oldact_idx, act_idx) { \
    struct sigaction *_oldact = (struct sigaction *) g_ptr_array_index(sigaction_oldact__captured, oldact_idx); \
    struct sigaction *_act = (struct sigaction *) g_ptr_array_index(sigaction_act__captured, act_idx); \
    fail_unless (memcmp(_oldact, _act, sizeof(struct sigaction)) == 0, \
        "sigaction(): oldact[%d] should be equals to act[%d]", oldact_idx, act_idx); \
}

/* @Verify */
#define VERIFY_SIGACTION_CALLS() { \
    mctest_assert_int_eq (sigaction_signum__captured->len, 6); \
\
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 0)), SIGINT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 1)), SIGQUIT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 2)), SIGTSTP); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 3)), SIGINT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 4)), SIGQUIT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(sigaction_signum__captured, 5)), SIGTSTP); \
\
    VERIFY_SIGACTION__ACT_IGNORED(g_ptr_array_index(sigaction_act__captured, 0)); \
    VERIFY_SIGACTION__ACT_IGNORED(g_ptr_array_index(sigaction_act__captured, 1)); \
    { \
        struct sigaction *_act  = g_ptr_array_index(sigaction_act__captured, 2); \
        fail_unless (memcmp (_act, &startup_handler, sizeof(struct sigaction)) == 0, \
            "The 'act' in third call to sigaction() should be equals to startup_handler"); \
    } \
\
    VERIFY_SIGACTION__IS_RESTORED (0, 3); \
    VERIFY_SIGACTION__IS_RESTORED (1, 4); \
    VERIFY_SIGACTION__IS_RESTORED (2, 5); \
\
    fail_unless (g_ptr_array_index(sigaction_oldact__captured, 3) == NULL, \
        "oldact in fourth call to sigaction() should be NULL"); \
    fail_unless (g_ptr_array_index(sigaction_oldact__captured, 4) == NULL, \
        "oldact in fifth call to sigaction() should be NULL"); \
    fail_unless (g_ptr_array_index(sigaction_oldact__captured, 5) == NULL, \
        "oldact in sixth call to sigaction() should be NULL"); \
}

/* --------------------------------------------------------------------------------------------- */

#define VERIFY_SIGNAL_HANDLER_IS_SIG_DFL(_idx) { \
    sighandler_t *tmp_handler = (sighandler_t *) g_ptr_array_index(signal_handler__captured, _idx);\
    mctest_assert_ptr_eq (tmp_handler, (sighandler_t *) SIG_DFL); \
}

/* @Verify */
#define VERIFY_SIGNAL_CALLS() { \
    mctest_assert_int_eq (signal_signum__captured->len, 4); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(signal_signum__captured, 0)), SIGINT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(signal_signum__captured, 1)), SIGQUIT); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(signal_signum__captured, 2)), SIGTSTP); \
    mctest_assert_int_eq (*((int *) g_ptr_array_index(signal_signum__captured, 3)), SIGCHLD); \
    \
    VERIFY_SIGNAL_HANDLER_IS_SIG_DFL (0); \
    VERIFY_SIGNAL_HANDLER_IS_SIG_DFL (1); \
    VERIFY_SIGNAL_HANDLER_IS_SIG_DFL (2); \
    VERIFY_SIGNAL_HANDLER_IS_SIG_DFL (3); \
}

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    signal__return_value = NULL;

    sigaction__init ();
    signal__init ();
    execvp__init ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    execvp__deinit ();
    signal__deinit ();
    sigaction__deinit ();
}

/* --------------------------------------------------------------------------------------------- */
