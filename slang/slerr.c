/* error handling common to all routines. */
/* Copyright (c) 1992, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

void (*SLang_VMessage_Hook) (char *, va_list);
void (*SLang_Error_Hook)(char *);
void (*SLang_Exit_Error_Hook)(char *, va_list);
volatile int SLang_Error = 0;
char *SLang_Error_Message;
volatile int SLKeyBoard_Quit = 0;

static char *get_error_string (void)
{
   char *str;

   if (!SLang_Error) SLang_Error = SL_UNKNOWN_ERROR;
   if (SLang_Error_Message != NULL) str = SLang_Error_Message;
   else switch(SLang_Error)
     {
      case SL_NOT_IMPLEMENTED: str = "Not Implemented"; break;
      case SL_APPLICATION_ERROR: str = "Application Error"; break;
      case SL_VARIABLE_UNINITIALIZED: str = "Variable Uninitialized"; break;
      case SL_MALLOC_ERROR : str = "Malloc Error"; break;
      case SL_INTERNAL_ERROR: str = "Internal Error"; break;
      case SL_STACK_OVERFLOW: str = "Stack Overflow"; break;
      case SL_STACK_UNDERFLOW: str = "Stack Underflow"; break;
      case SL_INTRINSIC_ERROR: str = "Intrinsic Error"; break;
      case SL_USER_BREAK: str = "User Break"; break;
      case SL_UNDEFINED_NAME: str = "Undefined Name"; break;
      case SL_SYNTAX_ERROR: str = "Syntax Error"; break;
      case SL_DUPLICATE_DEFINITION: str = "Duplicate Definition"; break;
      case SL_TYPE_MISMATCH: str = "Type Mismatch"; break;
      case SL_READONLY_ERROR: str = "Variable is read-only"; break;
      case SL_DIVIDE_ERROR: str = "Divide by zero"; break;
      case SL_OBJ_NOPEN: str = "Object not opened"; break;
      case SL_OBJ_UNKNOWN: str = "Object unknown"; break;
      case SL_INVALID_PARM: str = "Invalid Parameter"; break;
      case SL_TYPE_UNDEFINED_OP_ERROR:
	str = "Operation not defined for datatype"; break;
      case SL_USER_ERROR:
	str = "User Error"; break;
      case SL_USAGE_ERROR:
	str = "Illegal usage of function";
	break;
      case SL_FLOATING_EXCEPTION:
	str = "Floating Point Exception";
	break;
      case SL_UNKNOWN_ERROR:
      default: str = "Unknown Error Code";
     }

   SLang_Error_Message = NULL;
   return str;
}

void SLang_doerror (char *error)
{
   char *str = NULL;
   char *err;
   char *malloced_err_buf;
   char err_buf [1024];

   malloced_err_buf = NULL;

   if (((SLang_Error == SL_USER_ERROR)
	|| (SLang_Error == SL_USAGE_ERROR))
       && (error != NULL) && (*error != 0))
     err = error;
   else
     {
	char *sle = "S-Lang Error: ";
	unsigned int len = 0;
	char *fmt = "%s%s%s";

	str = get_error_string ();

	if ((error == NULL) || (*error == 0))
	  error = "";
	else if (SLang_Error == SL_UNKNOWN_ERROR)
	  /* Do not display an unknown error message if error is non-NULL */
	  str = "";
	else {
	  fmt = "%s%s: %s";
	  len = 2;	/* ": " */
	}

	len += strlen (sle) + strlen (str) + strlen(error) + 1 /* trailing 0 */;

	err = err_buf;
	if (len > sizeof (err_buf))
	  {
	     if (NULL == (malloced_err_buf = SLmalloc (len)))
	       err = NULL;
	     else
	       err = malloced_err_buf;
	  }

	if (err != NULL) sprintf (err, fmt, sle, str, error);
	else err = "Out of memory";
     }

   if (SLang_Error_Hook == NULL)
     {
	fputs (err, stderr);
	fputs("\r\n", stderr);
	fflush (stderr);
     }
   else
     (*SLang_Error_Hook)(err);

   SLfree (malloced_err_buf);
}

void SLang_verror (int err_code, char *fmt, ...)
{
   va_list ap;
   char err [1024];

   if (err_code == 0) err_code = SL_INTRINSIC_ERROR;
   if (SLang_Error == 0) SLang_Error = err_code;

   if (fmt != NULL)
     {
	va_start(ap, fmt);
	(void) _SLvsnprintf (err, sizeof (err), fmt, ap);
	fmt = err;
	va_end(ap);
     }

   SLang_doerror (fmt);
}

void SLang_exit_error (char *fmt, ...)
{
   va_list ap;

   va_start (ap, fmt);
   if (SLang_Exit_Error_Hook != NULL)
     {
	(*SLang_Exit_Error_Hook) (fmt, ap);
	exit (1);
     }

   if (fmt != NULL)
     {
	vfprintf (stderr, fmt, ap);
	fputs ("\r\n", stderr);
	fflush (stderr);
     }
   va_end (ap);

   exit (1);
}

void SLang_vmessage (char *fmt, ...)
{
   va_list ap;

   if (fmt == NULL)
     return;

   va_start (ap, fmt);

   if (SLang_VMessage_Hook != NULL)
     (*SLang_VMessage_Hook) (fmt, ap);
   else
     {
	vfprintf (stdout, fmt, ap);
	fputs ("\r\n", stdout);
     }

   va_end (ap);
}
