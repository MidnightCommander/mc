/* Copyright (c) 1998, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
#include "slinclud.h"

#include <signal.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include <errno.h>

#include "slang.h"
#include "_slang.h"

/* Do not trust these environments */
#if defined(__CYGWIN32__) || defined(__MINGW32__) || defined(AMIGA)
# ifdef SLANG_POSIX_SIGNALS
#  undef SLANG_POSIX_SIGNALS
# endif
#endif

/* This function will cause system calls to be restarted after signal if possible */
SLSig_Fun_Type *SLsignal (int sig, SLSig_Fun_Type *f)
{
#if defined(SLANG_POSIX_SIGNALS)
   struct sigaction old_sa, new_sa;

# ifdef SIGALRM
   /* We want system calls to be interrupted by SIGALRM. */
   if (sig == SIGALRM) return SLsignal_intr (sig, f);
# endif

   sigemptyset (&new_sa.sa_mask);
   new_sa.sa_handler = f;

   new_sa.sa_flags = 0;
# ifdef SA_RESTART
   new_sa.sa_flags |= SA_RESTART;
# endif

   if (-1 == sigaction (sig, &new_sa, &old_sa))
     return (SLSig_Fun_Type *) SIG_ERR;

   return old_sa.sa_handler;
#else
   /* Not POSIX. */
   return signal (sig, f);
#endif
}

/* This function will NOT cause system calls to be restarted after
 * signal if possible
 */
SLSig_Fun_Type *SLsignal_intr (int sig, SLSig_Fun_Type *f)
{
#ifdef SLANG_POSIX_SIGNALS
   struct sigaction old_sa, new_sa;

   sigemptyset (&new_sa.sa_mask);
   new_sa.sa_handler = f;

   new_sa.sa_flags = 0;
# ifdef SA_INTERRUPT
   new_sa.sa_flags |= SA_INTERRUPT;
# endif

   if (-1 == sigaction (sig, &new_sa, &old_sa))
     return (SLSig_Fun_Type *) SIG_ERR;

   return old_sa.sa_handler;
#else
   /* Not POSIX. */
   return signal (sig, f);
#endif
}

/* We are primarily interested in blocking signals that would cause the
 * application to reset the tty.  These include suspend signals and
 * possibly interrupt signals.
 */
#ifdef SLANG_POSIX_SIGNALS
static sigset_t Old_Signal_Mask;
#endif

static volatile unsigned int Blocked_Depth;

int SLsig_block_signals (void)
{
#ifdef SLANG_POSIX_SIGNALS
   sigset_t new_mask;
#endif

   Blocked_Depth++;
   if (Blocked_Depth != 1)
     {
	return 0;
     }

#ifdef SLANG_POSIX_SIGNALS
   sigemptyset (&new_mask);
# ifdef SIGQUIT
   sigaddset (&new_mask, SIGQUIT);
# endif
# ifdef SIGTSTP
   sigaddset (&new_mask, SIGTSTP);
# endif
# ifdef SIGINT
   sigaddset (&new_mask, SIGINT);
# endif
# ifdef SIGTTIN
   sigaddset (&new_mask, SIGTTIN);
# endif
# ifdef SIGTTOU
   sigaddset (&new_mask, SIGTTOU);
# endif
# ifdef SIGWINCH
   sigaddset (&new_mask, SIGWINCH);
# endif

   (void) sigprocmask (SIG_BLOCK, &new_mask, &Old_Signal_Mask);
   return 0;
#else
   /* Not implemented. */
   return -1;
#endif
}

int SLsig_unblock_signals (void)
{
   if (Blocked_Depth == 0)
     return -1;

   Blocked_Depth--;

   if (Blocked_Depth != 0)
     return 0;

#ifdef SLANG_POSIX_SIGNALS
   (void) sigprocmask (SIG_SETMASK, &Old_Signal_Mask, NULL);
   return 0;
#else
   return -1;
#endif
}

#if 0
#ifdef MSWINDOWS
int SLsystem (char *cmd)
{
   SLang_verror (SL_NOT_IMPLEMENTED, "system not implemented");
   return -1;
}

#else
int SLsystem (char *cmd)
{
#ifdef SLANG_POSIX_SIGNALS
   pid_t pid;
   int status;
   struct sigaction ignore;
# ifdef SIGINT
   struct sigaction save_intr;
# endif
# ifdef SIGQUIT
   struct sigaction save_quit;
# endif
# ifdef SIGCHLD
   sigset_t child_mask, save_mask;
# endif

   if (cmd == NULL) return 1;

   ignore.sa_handler = SIG_IGN;
   sigemptyset (&ignore.sa_mask);
   ignore.sa_flags = 0;

# ifdef SIGINT
   if (-1 == sigaction (SIGINT, &ignore, &save_intr))
     return -1;
# endif

# ifdef SIGQUIT
   if (-1 == sigaction (SIGQUIT, &ignore, &save_quit))
     {
	(void) sigaction (SIGINT, &save_intr, NULL);
	return -1;
     }
# endif

# ifdef SIGCHLD
   sigemptyset (&child_mask);
   sigaddset (&child_mask, SIGCHLD);
   if (-1 == sigprocmask (SIG_BLOCK, &child_mask, &save_mask))
     {
#  ifdef SIGINT
	(void) sigaction (SIGINT, &save_intr, NULL);
#  endif
#  ifdef SIGQUIT
	(void) sigaction (SIGQUIT, &save_quit, NULL);
#  endif
	return -1;
     }
# endif

   pid = fork();

   if (pid == -1)
     status = -1;
   else if (pid == 0)
     {
	/* Child */
# ifdef SIGINT
	(void) sigaction (SIGINT, &save_intr, NULL);
# endif
# ifdef SIGQUIT
	(void) sigaction (SIGQUIT, &save_quit, NULL);
# endif
# ifdef SIGCHLD
	(void) sigprocmask (SIG_SETMASK, &save_mask, NULL);
# endif

	execl ("/bin/sh", "sh", "-c", cmd, NULL);
	_exit (127);
     }
   else
     {
	/* parent */
	while (-1 == waitpid (pid, &status, 0))
	  {
# ifdef EINTR
	     if (errno == EINTR)
	       continue;
# endif
# ifdef ERESTARTSYS
	     if (errno == ERESTARTSYS)
	       continue;
# endif
	     status = -1;
	     break;
	  }
     }
# ifdef SIGINT
   if (-1 == sigaction (SIGINT, &save_intr, NULL))
     status = -1;
# endif
# ifdef SIGQUIT
   if (-1 == sigaction (SIGQUIT, &save_quit, NULL))
     status = -1;
# endif
# ifdef SIGCHLD
   if (-1 == sigprocmask (SIG_SETMASK, &save_mask, NULL))
     status = -1;
# endif

   return status;

#else				       /* No POSIX Signals */
# ifdef SIGINT
   void (*sint)(int);
# endif
# ifdef SIGQUIT
   void (*squit)(int);
# endif
   int status;

# ifdef SIGQUIT
   squit = SLsignal (SIGQUIT, SIG_IGN);
# endif
# ifdef SIGINT
   sint = SLsignal (SIGINT, SIG_IGN);
# endif
   status = system (cmd);
# ifdef SIGINT
   SLsignal (SIGINT, sint);
# endif
# ifdef SIGQUIT
   SLsignal (SIGQUIT, squit);
# endif
   return status;
#endif				       /* POSIX_SIGNALS */
}
#endif

#include <windows.h>
static int msw_system (char *cmd)
{
   STARTUPINFO startup_info;
   PROCESS_INFORMATION process_info;
   int status;

   if (cmd == NULL) return -1;

   memset ((char *) &startup_info, 0, sizeof (STARTUPINFO));
   startup_info.cb = sizeof(STARTUPINFO);
   startup_info.dwFlags = STARTF_USESHOWWINDOW;
   startup_info.wShowWindow = SW_SHOWDEFAULT;

   if (FALSE == CreateProcess (NULL,
			       cmd,
			       NULL,
			       NULL,
			       FALSE,
			       NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE,
			       NULL,
			       NULL,
			       &startup_info,
			       &process_info))
     {
	SLang_verror (0, "%s: CreateProcess failed.", cmd);
	return -1;
     }

   status = -1;

   if (0xFFFFFFFFUL != WaitForSingleObject (process_info.hProcess, INFINITE))
     {
	DWORD exit_code;

	if (TRUE == GetExitCodeProcess (process_info.hProcess, &exit_code))
	  status = (int) exit_code;
     }

   CloseHandle (process_info.hThread);
   CloseHandle (process_info.hProcess);

   return status;
}
#endif
