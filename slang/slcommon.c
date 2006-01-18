/* This file contains library-wide symbols that are always needed when one
 * links to the library.
 */
/*
Copyright (C) 2004, 2005 John E. Davis

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
#define _GNU_SOURCE
#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef HAVE_LANGINFO_H
# include <langinfo.h>
#endif

#define DEBUG_MALLOC 0

#if DEBUG_MALLOC
# define SLREALLOC_FUN	SLdebug_realloc
# define SLMALLOC_FUN	SLdebug_malloc
# define SLFREE_FUN	SLdebug_free
#else
# define SLREALLOC_FUN	SLREALLOC
# define SLMALLOC_FUN	SLMALLOC
# define SLFREE_FUN	SLFREE
#endif


int SLang_Version = SLANG_VERSION;
char *SLang_Version_String = SLANG_VERSION_STRING;

int _pSLinterp_UTF8_Mode = 0;
int _pSLtt_UTF8_Mode = 0;
int _pSLutf8_mode = 0;

#ifndef HAVE_LOCALE_H
# define setlocale(x,y) (NULL)
# define LC_ALL 0
#endif

int SLutf8_is_utf8_mode (void)
{
   return _pSLutf8_mode;
}

int SLinterp_utf8_enable (int mode)
{
   if (mode == -1)
     mode = _pSLutf8_mode;

   return _pSLinterp_UTF8_Mode = mode;
}

int SLinterp_is_utf8_mode (void)
{
   return _pSLinterp_UTF8_Mode;
}

static int utf8_enable (int mode)
{
   char *locale;

   if (mode != -1)
     return (mode != 0);

   (void) setlocale (LC_ALL, "");

#ifdef HAVE_NL_LANGINFO
   locale = nl_langinfo (CODESET);
   if ((locale != NULL) && (*locale))
     {
	if ((0 == strcmp (locale, "UTF-8"))
	    || (0 == strcmp (locale, "utf-8"))
	    || (0 == strcmp (locale, "utf8"))
	    || (0 == strcmp (locale, "UTF8")))
	  return 1;
	
	return 0;
     }
#endif

   locale = setlocale (LC_ALL, "");

   if (((locale == NULL) || (*locale == 0))
       && ((NULL == (locale = getenv ("LC_ALL"))) || (*locale = 0))
       && ((NULL == (locale = getenv ("LC_CTYPE"))) || (*locale = 0))
       && ((NULL == (locale = getenv ("LANG"))) || (*locale = 0)))
     return 0;

   /* setlocale man page says the return value is something like:
    *   language[_territory][.codeset][@modifier][+special][,...
    * Here, we want the codeset, if present.
    */
   
   while (*locale && (*locale != '.') && (*locale != '@')
	  && (*locale != '+') && (*locale != ','))
     locale++;
   
   if (*locale == '.')
     {
	locale++;
	if (0 == strncmp (locale, "UTF-8", 5))
	  locale += 5;
	else if (0 == strncmp (locale, "utf8", 4))
	  locale += 4;
	else
	  return 0;
	
	if ((*locale == 0) || (*locale == '@') 
	    || (*locale == '+') || (*locale == ','))
	  return 1;
     }
   
   return 0;
}

  
/* Returns the value of _pSLutf8_mode */
int SLutf8_enable (int mode)
{
   mode = utf8_enable (mode);
   _pSLutf8_mode = mode;
   _pSLtt_UTF8_Mode = mode;
   _pSLinterp_UTF8_Mode = mode;
   return mode;
}

#ifndef MIDNIGHT_COMMANDER_CODE

char *SLmalloc (unsigned int len)
{
   char *p;

   p = (char *) SLMALLOC_FUN (len);
   if (p == NULL)
     SLang_set_error (SL_MALLOC_ERROR);

   return p;
}

void SLfree (char *p)
{
   if (p != NULL) SLFREE_FUN (p);
}

char *SLrealloc (char *p, unsigned int len)
{
   if (len == 0)
     {
	SLfree (p);
	return NULL;
     }

   if (p == NULL) p = SLmalloc (len);
   else
     {
	p = (char *)SLREALLOC_FUN (p, len);
	if (p == NULL)
	  SLang_set_error (SL_MALLOC_ERROR);
     }
   return p;
}

#endif                                /* !MIDNIGHT_COMMANDER_CODE */

char *SLcalloc (unsigned int nelems, unsigned int len)
{
   char *p;

   len = nelems * len;
   p = SLmalloc (len);
   if (p != NULL) memset (p, 0, len);
   return p;
}

#if 0 /* was: !defined(HAVE_ISSETUGID) && defined(__GLIBC__) && (__GLIBC__ >= 2) */
extern int __libc_enable_secure;
# define HAVE___LIBC_ENABLE_SECURE 1
#endif

int _pSLsecure_issetugid (void)
{
#ifdef HAVE_ISSETUGID
   return (1 == issetugid ());
#else
# if defined HAVE___LIBC_ENABLE_SECURE && 0
   return __libc_enable_secure;
# else
#  if defined(HAVE_GETUID) && defined(HAVE_GETEUID) && defined(HAVE_GETGID) && defined(HAVE_GETEUID)
   static int enable_secure;
   if (enable_secure == 0)
    {
       if ((getuid () != geteuid ()) 
	   || (getgid () != getegid ()))
	 enable_secure = 1;
       else
	 enable_secure = -1;
    }
   return (enable_secure == 1);
#  else
   return 0;
#  endif
# endif
#endif
}

/* Like getenv, except if running as setuid or setgid, returns NULL */
char *_pSLsecure_getenv (char *s)
{
   if (_pSLsecure_issetugid ())
     return NULL;
   return getenv (s);
}


typedef struct Interrupt_Hook_Type
{
   int (*func)(VOID_STAR);
   VOID_STAR client_data;
   struct Interrupt_Hook_Type *next;
}
Interrupt_Hook_Type;

static Interrupt_Hook_Type *Interrupt_Hooks = NULL;

static Interrupt_Hook_Type *
  find_interrupt_hook (int (*func)(VOID_STAR), VOID_STAR cd,
		       Interrupt_Hook_Type **prevp)
{
   Interrupt_Hook_Type *h = Interrupt_Hooks;
   Interrupt_Hook_Type *prev = NULL;
   while (h != NULL)
     {
	if ((h->func == func) && (h->client_data == cd))
	  {
	     if (prevp != NULL)
	       *prevp = prev;
	     return h;
	  }
	h = h->next;
     }
   return NULL;
}
   
int SLang_add_interrupt_hook (int (*func)(VOID_STAR), VOID_STAR cd)
{
   Interrupt_Hook_Type *h;

   if (NULL != find_interrupt_hook (func, cd, NULL))
     return 0;
   
   if (NULL == (h = (Interrupt_Hook_Type *)SLmalloc (sizeof (Interrupt_Hook_Type))))
     return -1;
   
   h->func = func;
   h->client_data = cd;
   h->next = Interrupt_Hooks;
   Interrupt_Hooks = h;
   return 0;
}

void SLang_remove_interrupt_hook (int (*func)(VOID_STAR), VOID_STAR cd)
{
   Interrupt_Hook_Type *h, *hprev;

   if (NULL == (h = find_interrupt_hook (func, cd, &hprev)))
     return;
   
   if (hprev == NULL)
     Interrupt_Hooks = h->next;
   else
     hprev->next = h->next;
   
   SLfree ((char *) h);
}

int SLang_handle_interrupt (void)
{
   Interrupt_Hook_Type *h;
   int status = 0;

   h = Interrupt_Hooks;
   while (h != NULL)
     {
	if (-1 == (*h->func)(h->client_data))
	  status = -1;

	h = h->next;
     }
   return status;
}

#if defined(__WIN32__) && defined(SLANG_DLL)
# include <windows.h>

BOOL WINAPI DllMain(HANDLE hInstance,DWORD dwReason,LPVOID lpParam)
{
   return 1;
}
#endif
