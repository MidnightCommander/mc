/*
Copyright (C) 2004, 2005 John E. Davis

This file is part of the S-Lang Library.

Trimmed down for use in GNU Midnight Commander.

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
int SLang_Version = SLANG_VERSION;

/*
 * This function assumes that the initial \ char has been removed.
 */
char *_pSLexpand_escaped_char(char *p, SLwchar_Type *ch, int *isunicodep)
{
   int i = 0;
   SLwchar_Type max = 0;
   SLwchar_Type num, base = 0;
   SLwchar_Type ch1;
   int isunicode;

   ch1 = *p++;
   isunicode = 0;

   switch (ch1)
     {
      default: num = ch1; break;
      case 'n': num = '\n'; break;
      case 't': num = '\t'; break;
      case 'v': num = '\v'; break;
      case 'b': num = '\b'; break;
      case 'r': num = '\r'; break;
      case 'f': num = '\f'; break;
      case 'E': case 'e': num = 27; break;
      case 'a': num = 7;
	break;

	/* octal */
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
	max = '7';
	base = 8; i = 2; num = ch1 - '0';
	break;

      case 'd':			       /* decimal -- S-Lang extension */
	base = 10;
	i = 3;
	max = '9';
	num = 0;
	break;

      case 'x':			       /* hex */
	base = 16;
	max = '9';
	i = 2;
	num = 0;
	
	if (*p == '{')
	  {
	     p++;
	     i = 0;
	     while (p[i] && (p[i] != '}'))
	       i++;
	     if (p[i] != '}')
	       {
		  SLang_verror (SL_UNICODE_ERROR, "Escaped unicode character missing closing }.");
		  return NULL;
	       }
	     isunicode = 1;
	  }
	break;
     }

   while (i)
     {
	ch1 = *p;

	i--;

	if ((ch1 <= max) && (ch1 >= '0'))
	  {
	     num = base * num + (ch1 - '0');
	  }
	else if (base == 16)
	  {
	     ch1 |= 0x20;
	     if ((ch1 < 'a') || ((ch1 > 'f'))) break;
	     num = base * num + 10 + (ch1 - 'a');
	  }
	else break;
	p++;
     }

   if (isunicode)
     {
	if (*p != '}')
	  {
	     SLang_verror (SL_UNICODE_ERROR, "Malformed Escaped unicode character.");
	     return NULL;
	  }
	p++;
     }

   if (isunicodep != NULL)
     *isunicodep = isunicode;

   *ch = num;
   return p;
}
