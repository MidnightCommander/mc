/* error handling common to all routines. */
/*
Copyright (C) 2004, 2005, 2006 John E. Davis

This file is part of the S-Lang Library.

The S-Lang Library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The S-Lang Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.  
*/

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

void (*_pSLinterpreter_Error_Hook) (int);

void (*SLang_VMessage_Hook) (char *, va_list);
void (*SLang_Error_Hook)(char *);
void (*SLang_Exit_Error_Hook)(char *, va_list);
void (*SLang_Dump_Routine)(char *);

volatile int _pSLang_Error = 0;
volatile int SLKeyBoard_Quit = 0;

typedef struct _Exception_Type Exception_Type;
struct _Exception_Type
{
   int error_code;
   char *name;
   char *description;
   Exception_Type *subclasses;
   Exception_Type *next;
   Exception_Type *parent;
};

static Exception_Type *Exception_Root;
static Exception_Type Exception_Root_Buf = 
{
   -1, "AnyError", "All Errors", NULL, NULL, NULL
};


/* Built-in error codes */
/* These values should correspond to the values produced by _pSLerr_init.
 * Some apps may not use the interpreter, and as such _pSLerr_init will not
 * get called.
 */
int SL_Any_Error = -1;
int SL_Unknown_Error = 6;
int SL_Internal_Error = 5;
int SL_OS_Error = 1;
int   SL_Malloc_Error = 2;
int   SL_Import_Error = 7;
int SL_RunTime_Error = 3;
int   SL_InvalidParm_Error = 4;
int   SL_TypeMismatch_Error = 8;
int   SL_UserBreak_Error = 9;
int   SL_Stack_Error = 10;
int     SL_StackOverflow_Error = 12;
int     SL_StackUnderflow_Error = 11;
int   SL_ReadOnly_Error = 13;
int   SL_VariableUninitialized_Error = 14;
int   SL_NumArgs_Error = 15;
int   SL_Index_Error = 16;
int   SL_Usage_Error = 17;
int   SL_Application_Error = 18;
int   SL_NotImplemented_Error = 19;
int   SL_LimitExceeded_Error = 20;
int   SL_Forbidden_Error = 21;
int   SL_Math_Error = 22;
int     SL_DivideByZero_Error = 23;
int     SL_ArithOverflow_Error = 24;
int     SL_ArithUnderflow_Error = 25;
int     SL_Domain_Error = 26;
int   SL_IO_Error = 27;
int     SL_Write_Error = 28;
int     SL_Read_Error = 29;
int     SL_Open_Error = 30;
int   SL_Data_Error = 31;
int   SL_Unicode_Error = 32;
int     SL_InvalidUTF8_Error = 33;
int   SL_Namespace_Error = 34;
int SL_Parse_Error = 35;
int   SL_Syntax_Error = 36;
int   SL_DuplicateDefinition_Error = 37;
int   SL_UndefinedName_Error = 38;

typedef struct
{
   int *errcode_ptr;
   char *name;
   char *description;
   int *base_class_ptr;
}
BuiltIn_Exception_Table_Type;

static SLCONST BuiltIn_Exception_Table_Type BuiltIn_Exception_Table[] =
{
   /* Define MallocError and InvalidParmError ASAP */
     {&SL_OS_Error, "OSError", "OS Error", &SL_Any_Error},
       {&SL_Malloc_Error, "MallocError", "Not enough memory", &SL_OS_Error},

     {&SL_RunTime_Error, "RunTimeError", "Run-Time Error", &SL_Any_Error},
       {&SL_InvalidParm_Error, "InvalidParmError", "Invalid Parameter", &SL_RunTime_Error},

     {&SL_Internal_Error, "InternalError", "Internal Error", &SL_Any_Error},
     {&SL_Unknown_Error, "UnknownError", "Unknown Error", &SL_Any_Error},

   /* Rest of OSErrors */
       {&SL_Import_Error, "ImportError", "Import Error", &SL_OS_Error},

   /* Rest of RunTimeErrors */
       {&SL_TypeMismatch_Error, "TypeMismatchError", "Type Mismatch", &SL_RunTime_Error},
       {&SL_UserBreak_Error, "UserBreakError", "User Break", &SL_RunTime_Error},
       {&SL_Stack_Error, "StackError", "Stack Error", &SL_RunTime_Error},
         {&SL_StackUnderflow_Error, "StackUnderflowError", "Stack Underflow Error", &SL_Stack_Error},
         {&SL_StackOverflow_Error, "StackOverflowError", "Stack Overflow Error", &SL_Stack_Error},
       {&SL_ReadOnly_Error, "ReadOnlyError", "Read-Only Error", &SL_RunTime_Error},
       {&SL_VariableUninitialized_Error, "VariableUninitializedError", "Variable Uninitialized Error", &SL_RunTime_Error},
       {&SL_NumArgs_Error, "NumArgsError", "Invalid Number of Arguments", &SL_RunTime_Error},
       {&SL_Index_Error, "IndexError", "Invalid Index", &SL_RunTime_Error},
       {&SL_Usage_Error, "UsageError", "Illegal Usage", &SL_RunTime_Error},
       {&SL_Application_Error, "ApplicationError", "Application Error", &SL_RunTime_Error},
       {&SL_NotImplemented_Error, "NotImplementedError", "Not Implemented", &SL_RunTime_Error},
       {&SL_LimitExceeded_Error, "LimitExceededError", "Limit Exceeded", &SL_RunTime_Error},
       {&SL_Forbidden_Error, "ForbiddenError", "Operation Forbidden", &SL_RunTime_Error},
       {&SL_Math_Error, "MathError", "Math Error", &SL_RunTime_Error},
         {&SL_DivideByZero_Error, "DivideByZeroError", "Divide by Zero", &SL_Math_Error},
         {&SL_ArithOverflow_Error, "ArithOverflowError", "Arithmetic Overflow", &SL_Math_Error},
         {&SL_ArithUnderflow_Error, "ArithUnderflowError", "Arithmetic Underflow", &SL_Math_Error},
         {&SL_Domain_Error, "DomainError", "Domain Error", &SL_Math_Error},
       {&SL_IO_Error, "IOError", "I/O Error", &SL_RunTime_Error},
         {&SL_Write_Error, "WriteError", "Write failed", &SL_IO_Error},
         {&SL_Read_Error, "ReadError", "Read failed", &SL_IO_Error},
         {&SL_Open_Error, "OpenError", "Open failed", &SL_IO_Error},
       {&SL_Data_Error, "DataError", "Data Error", &SL_RunTime_Error},
       {&SL_Unicode_Error, "UnicodeError", "Unicode Error", &SL_RunTime_Error},
         {&SL_InvalidUTF8_Error, "UTF8Error", "Invalid UTF8", &SL_Unicode_Error},
       {&SL_Namespace_Error, "NamespaceError", "Namespace Error", &SL_RunTime_Error},

   /* Parse Errors */
       {&SL_Parse_Error, "ParseError", "Parse Error", &SL_Any_Error},
         {&SL_Syntax_Error, "SyntaxError", "Syntax Error", &SL_Parse_Error},
         {&SL_DuplicateDefinition_Error, "DuplicateDefinitionError", "Duplicate Definition", &SL_Parse_Error},
         {&SL_UndefinedName_Error, "UndefinedNameError", "Undefined Name", &SL_Parse_Error},
   {NULL, NULL, NULL, NULL}
};

static Exception_Type *find_exception (Exception_Type *root, int error_code)
{
   Exception_Type *e;

   while (root != NULL)
     {
	if (error_code == root->error_code)
	  return root;

	if (root->subclasses != NULL)
	  {
	     e = find_exception (root->subclasses, error_code);
	     if (e != NULL)
	       return e;
	  }
	root = root->next;
     }

   return root;
}

static int is_exception_ancestor (int a, int b)
{
   Exception_Type *e;

   if (a == b)
     return 1;

   if (NULL == (e = find_exception (Exception_Root, a)))
     return 0;
   
   while (e->parent != NULL)
     {
	e = e->parent;
	if (e->error_code == b)
	  return 1;
     }
   return 0;
}

int SLerr_exception_eqs (int a, int b)
{
   if (is_exception_ancestor (a, b))
     return 1;
   
   return 0;
}

static void free_this_exception (Exception_Type *e)
{
   if (e == NULL)
     return;
   
   if (e->name != NULL)
     SLang_free_slstring (e->name);

   if (e->description != NULL)
     SLang_free_slstring (e->description);

   SLfree ((char *)e);
}

	
static int Next_Exception_Code;
/* The whole point of this nonsense involving the _pSLerr_New_Exception_Hook
 * is to provide a mechanism to avoid linking in the interpreter for apps
 * that just want the other facilities.
 */
int (*_pSLerr_New_Exception_Hook)(char *name, char *desc, int error_code);

int _pSLerr_init_interp_exceptions (void)
{
   SLCONST BuiltIn_Exception_Table_Type *b;
   Exception_Type *e;

   if (_pSLerr_New_Exception_Hook == NULL)
     return 0;
   
   e = &Exception_Root_Buf;
   if (-1 == (*_pSLerr_New_Exception_Hook)(e->name, e->description, e->error_code))
     return -1;

   b = BuiltIn_Exception_Table;
   while (b->errcode_ptr != NULL)
     {
	if (-1 == (*_pSLerr_New_Exception_Hook)(b->name, b->description, *b->errcode_ptr))
	  return -1;
	
	b++;
     }
   return 0;
}

int SLerr_new_exception (int baseclass, char *name, char *descript)
{
   Exception_Type *base;
   Exception_Type *e;

   if (-1 == _pSLerr_init ())
     return -1;

   base = find_exception (Exception_Root, baseclass);
   if (base == NULL)
     {
	SLang_verror (SL_InvalidParm_Error,
		      "Base class for new exception not found");
	return -1;
     }
   
   e = (Exception_Type *) SLcalloc (1, sizeof (Exception_Type));
   if (e == NULL)
     return -1;
   
   if ((NULL == (e->name = SLang_create_slstring (name)))
       || (NULL == (e->description = SLang_create_slstring (descript))))
     {
	free_this_exception (e);
	return -1;
     }
   
   e->error_code = Next_Exception_Code;

   if ((_pSLerr_New_Exception_Hook != NULL)
       && (-1 == (*_pSLerr_New_Exception_Hook) (e->name, e->description, e->error_code)))
     {
	free_this_exception (e);
	return -1;
     }

   e->parent = base;
   e->next = base->subclasses;
   base->subclasses = e;

   Next_Exception_Code++;
   return e->error_code;
}


static int init_exceptions (void)
{
   SLCONST BuiltIn_Exception_Table_Type *b;

   if (Exception_Root != NULL)
     return 0;

   Exception_Root = &Exception_Root_Buf;
   Next_Exception_Code = 1;
   b = BuiltIn_Exception_Table;
   while (b->errcode_ptr != NULL)
     {
	int err_code;
	
	err_code = SLerr_new_exception (*b->base_class_ptr, b->name, b->description);
	if (err_code == -1)
	  return -1;

	*b->errcode_ptr = err_code;
	b++;
     }
   
   return 0;
}

static void free_exceptions (Exception_Type *root)
{
   while (root != NULL)
     {
	Exception_Type *next;

	if (root->subclasses != NULL)
	  free_exceptions (root->subclasses);
	
	next = root->next;
	free_this_exception (root);
	root = next;
     }
}

	
static void deinit_exceptions (void)
{
   Exception_Type *root = Exception_Root;
   
   if (root != NULL)
     free_exceptions (root->subclasses);
   
   Exception_Root = NULL;
   Next_Exception_Code = 0;
}

char *SLerr_strerror (int err_code)
{
   Exception_Type *e;

   if (err_code == 0)
     err_code = _pSLang_Error;

   if (-1 == _pSLerr_init ())
     return "Unable to initialize SLerr module";

   if (NULL == (e = find_exception (Exception_Root, err_code)))
     return "Invalid/Unknown Error Code";
   
   return e->description;
}

/* Error Queue Functions
 *   SLang_verror (int errcode, fmt, args)
 *     Add an error message to the queue.
 *   SLerr_delete_queue ()
 *     Removes messages from the error queue
 *   SLerr_print_queue ()
 *     Prints all messages from the queue, deletes the queue
 */
typedef struct _Error_Message_Type
{
   char *msg;			       /* SLstring, may be NULL */
   int msg_type;
#define _SLERR_MSG_ERROR	1
#define _SLERR_MSG_WARNING	2
#define _SLERR_MSG_TRACEBACK	4
   struct _Error_Message_Type *next;
}
Error_Message_Type;

typedef struct
{
   Error_Message_Type *head;
   Error_Message_Type *tail;
}
Error_Queue_Type;

static Error_Queue_Type *Default_Error_Queue;

static void free_error_msg (Error_Message_Type *m)
{
   if (m == NULL)
     return;
   if (m->msg != NULL)
     SLang_free_slstring (m->msg);
   SLfree ((char *)m);
}
       
static Error_Message_Type *allocate_error_msg (char *msg, int msg_type)
{
   Error_Message_Type *m;

   if (NULL == (m = (Error_Message_Type*) SLcalloc (1, sizeof (Error_Message_Type))))
     return NULL;
   
   if ((NULL != msg) && (NULL == (m->msg = SLang_create_slstring (msg))))
     {
	free_error_msg (m);
	return NULL;
     }
   m->msg_type = msg_type;
   return m;
}

static void free_queued_messages (Error_Queue_Type *q)
{
   Error_Message_Type *m;

   if (q == NULL)
     return;

   m = q->head;
   while (m != NULL)
     {
	Error_Message_Type *m1 = m->next;
	free_error_msg (m);
	m = m1;
     }
   q->head = NULL;
   q->tail = NULL;
}

static void delete_msg_queue (Error_Queue_Type *q)
{
   if (q == NULL)
     return;
   
   free_queued_messages (q);
   SLfree ((char *)q);
}

   
static Error_Queue_Type *create_msg_queue (void)
{
   Error_Queue_Type *q;

   if (NULL == (q = (Error_Queue_Type *)SLcalloc (1, sizeof(Error_Queue_Type))))
     return NULL;
   
   return q;
}

static int queue_message (Error_Queue_Type *q, char *msg, int msg_type)
{
   Error_Message_Type *m;
   
   if (NULL == (m = allocate_error_msg (msg, msg_type)))
     return -1;
   
   if (q->tail != NULL)
     q->tail->next = m;
   if (q->head == NULL)
     q->head = m;
   q->tail = m;
   
   return 0;
}

static void print_error (int msg_type, char *err)
{
   unsigned int len;

   switch (msg_type)
     {
      case _SLERR_MSG_ERROR:
	if (SLang_Error_Hook != NULL)
	  {
	     (*SLang_Error_Hook)(err);
	     return;
	  }
	break;
      case _SLERR_MSG_TRACEBACK:
      case _SLERR_MSG_WARNING:
	if (SLang_Dump_Routine != NULL)
	  {
	     (*SLang_Dump_Routine)(err);
	     return;
	  }
	break;
     }
   
   len = strlen (err);
   if (len == 0)
     return;

   fputs (err, stderr);
   if ((err[len-1] != '\n')
       && (msg_type != _SLERR_MSG_TRACEBACK))
     fputs("\n", stderr);

   fflush (stderr);
}

static void print_queue (void)
{
   if (-1 == _pSLerr_init ())
     print_error (_SLERR_MSG_ERROR, "Unable to initialize SLerr module");
   
   if (_pSLang_Error == 0)
     return;

   if (Default_Error_Queue != NULL)
     {
	Error_Queue_Type *q = Default_Error_Queue;
	Error_Message_Type *m = q->head;
	while (m != NULL)
	  {
	     Error_Message_Type *m_next = m->next;
	     if (m->msg != NULL)
	       print_error (m->msg_type, m->msg);
	     m = m_next;
	  }
	
	free_queued_messages (q);
     }
#if 0
   if (_pSLang_Error != SL_Usage_Error)
     {
	print_error (_SLERR_MSG_ERROR, SLerr_strerror (_pSLang_Error));
     }
#endif
}

/* This function returns a pointer to the first error message in the queue.
 * Make no attempts to free the returned pointer.
 */
char *_pSLerr_get_error_from_queue (void)
{
   Error_Queue_Type *q;
   Error_Message_Type *m;
   unsigned int len;
   char *err, *err1, *err_max;

   if (NULL == (q = Default_Error_Queue))
     return NULL;

   len = 0;
   m = q->head;
   while (m != NULL)
     {
	if (m->msg_type == _SLERR_MSG_ERROR)
	  len += 1 + strlen (m->msg);

	m = m->next;
     }
   
   if (len) 
     len--;			       /* last \n not needed */

   if (NULL == (err = _pSLallocate_slstring (len)))
     return NULL;
   
   err_max = err + len;
   err1 = err;
   m = q->head;
   while (m != NULL)
     {
	if (m->msg_type == _SLERR_MSG_ERROR)
	  {
	     unsigned int dlen = strlen (m->msg);
	     strcpy (err1, m->msg);
	     err1 += dlen;
	     if (err1 != err_max)
	       *err1++ = '\n';
	  }
	m = m->next;
     }
   *err1 = 0;
   
   return _pSLcreate_via_alloced_slstring (err, len);
}

void _pSLerr_print_message_queue (void)
{
   print_queue ();
}


static volatile int Suspend_Error_Messages = 0;
int _pSLerr_resume_messages (void)
{
   if (Suspend_Error_Messages == 0)
     return 0;
   
   Suspend_Error_Messages--;
   if (Suspend_Error_Messages == 0)
     print_queue ();
   return 0;
}

int _pSLerr_suspend_messages (void)
{
   Suspend_Error_Messages++;
   return 0;
}

void _pSLerr_free_queued_messages (void)
{
   free_queued_messages (Default_Error_Queue);
}


void SLang_verror (int err_code, char *fmt, ...)
{
   va_list ap;
   char err [4096];

   if (-1 == _pSLerr_init ())
     {
	print_queue ();
	return;
     }

   if (err_code == 0) 
     err_code = SL_INTRINSIC_ERROR;

   if (_pSLang_Error == 0)
     SLang_set_error (err_code);

   if (fmt == NULL)
     return;

   va_start(ap, fmt);
   (void) SLvsnprintf (err, sizeof (err), fmt, ap);
   va_end(ap);

   if (Suspend_Error_Messages)
     (void) queue_message (Default_Error_Queue, err, _SLERR_MSG_ERROR);
   else
     print_error (_SLERR_MSG_ERROR, err);
}

int _pSLerr_traceback_msg (char *fmt, ...)
{
   va_list ap;
   char msg [4096];

   va_start(ap, fmt);
   (void) SLvsnprintf (msg, sizeof (msg), fmt, ap);
   va_end(ap);

   return queue_message (Default_Error_Queue, msg, _SLERR_MSG_TRACEBACK);
}

void SLang_exit_error (char *fmt, ...)
{
   va_list ap;

   print_queue ();
   va_start (ap, fmt);
   if (SLang_Exit_Error_Hook != NULL)
     {
	(*SLang_Exit_Error_Hook) (fmt, ap);
	exit (1);
     }

   if (fmt != NULL)
     {
	vfprintf (stderr, fmt, ap);
	fputs ("\n", stderr);
	fflush (stderr);
     }
   va_end (ap);

   exit (1);
}

int SLang_set_error (int error)
{
   /* Only allow an error to be cleared (error==0), but not changed
    * if there already is an error.
    */
   if ((error == 0)
       || (_pSLang_Error == 0))
     _pSLang_Error = error;

   if (_pSLinterpreter_Error_Hook != NULL)
     (*_pSLinterpreter_Error_Hook) (_pSLang_Error);
   
   return 0;
}

int SLang_get_error (void)
{
   return _pSLang_Error;
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
	fputs ("\n", stdout);
     }

   va_end (ap);
}

/* This routine does not queue messages.  It is used for tracing, etc. */
void _pSLerr_dump_msg (char *fmt, ...)
{
   char buf[1024];
   va_list ap;

   va_start (ap, fmt);
   if (SLang_Dump_Routine != NULL)
     {
	(void) SLvsnprintf (buf, sizeof (buf), fmt, ap);
	(*SLang_Dump_Routine) (buf);
     }
   else
     {
	vfprintf (stderr, fmt, ap);
	fflush (stderr);
     }
   va_end (ap);
}

int _pSLerr_init (void)
{
   if (Default_Error_Queue == NULL)
     {
	Suspend_Error_Messages = 0;
	if (NULL == (Default_Error_Queue = create_msg_queue ()))
	  return -1;
     }

   if (-1 == init_exceptions ())
     return -1;
   
   return 0;
}

void _pSLerr_deinit (void)
{
   deinit_exceptions ();
   delete_msg_queue (Default_Error_Queue);
   Suspend_Error_Messages = 0;
   Default_Error_Queue = NULL;
}

