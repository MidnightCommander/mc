/* error handling common to all routines. */
/* Copyright (c) 1992, 1995 John E. Davis
 * All rights reserved.
 * 
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */


#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include "_slang.h"

void (*SLang_Error_Routine)(char *);
void (*SLang_Exit_Error_Hook)(char *);
volatile int SLang_Error = 0;
char *SLang_Error_Message;
volatile int SLKeyBoard_Quit = 0;

void SLang_doerror (char *error)
{
   if (SLang_Error_Routine == NULL)
     {
	fprintf (stderr, "S-Lang Error: %s\r\n", error);
     }
   else
     (*SLang_Error_Routine)(error);
}



void SLang_exit_error (char *s)
{
   if (SLang_Exit_Error_Hook != NULL)
     {
	(*SLang_Exit_Error_Hook) (s);
     }
   if (s != NULL) fprintf (stderr, "%s\n", s);
   exit (-1);
}
