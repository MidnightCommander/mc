/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "includes.h"

#ifndef MAXUNI
#define MAXUNI 1024
#endif

/*******************************************************************
create a null-terminated unicode string from a null-terminated ascii string.
return number of unicode chars copied, excluding the null character.
only handles ascii strings
Unicode strings created are in little-endian format.
********************************************************************/

int struni2(char *dst, const char *src)
{
	size_t len = 0;

	if (dst == NULL)
		return 0;

	if (src != NULL)
	{
		for (; *src && len < MAXUNI-2; len++, dst +=2, src++)
		{
			SSVAL(dst,0,(*src) & 0xFF);
		}
	}

	SSVAL(dst,0,0);

	return len;
}

