/* XView error handling routines.
   Copyright (C) 1995 Jakub Jelinek.
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <config.h>

#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/notice.h>

extern Frame mcframe;

int xv_error_handler (Xv_object object, Attr_avlist avlist)
{
    Attr_avlist attrs;
    Error_severity severity = ERROR_RECOVERABLE;
    int n = 0;
    char str1[64], str2[64], str3[64], str4[64], str5[64], str6[64], str7[64];
    char *strs [7];
    
    strs [0] = str1; strs [1] = str2; strs [2] = str3; strs [3] = str4; 
    strs [4] = str5; strs [5] = str6; strs [6] = str7; 
    for (attrs = avlist; *attrs && n < 5; attrs = attr_next (attrs)) {
        switch ((int) attrs [0]) {
            case ERROR_BAD_ATTR:
            	sprintf (strs [n++], "bad attribute %s",
            	    attr_name (attrs [1]));
            	break;
            case ERROR_BAD_VALUE:
            	sprintf (strs [n++], "bad value (0x%x) for attribute %s",
            	    (int) attrs [1], attr_name (attrs [2]));
            	break;
            case ERROR_INVALID_OBJECT:
            	sprintf (strs [n++], "invalid object (%s)",
            	    (char *) attrs [1]);
            	break;
            case ERROR_STRING:
            	sprintf (strs [n++], "%s",
            	    (char *) attrs [1]);
            	break;
            case ERROR_PKG:
            	sprintf (strs [n++], "Package: %s",
            	    ((Xv_pkg *) attrs [1])->name);
            	break;
            case ERROR_SEVERITY:
            	severity = attrs [1];
        }
    }
    
    strcpy (strs [n++], "Dump core?");
    strs [n] = NULL;
#ifdef NICE_DEBUG
    if (notice_prompt (mcframe, (Event *) NULL,
        NOTICE_MESSAGE_STRINGS_ARRAY_PTR, strs,
        NOTICE_BUTTON_YES, "Yes",
        NOTICE_BUTTON_NO, "No",
        NULL) == NOTICE_YES)
            abort ();
#endif            
    if (severity == ERROR_NON_RECOVERABLE)
    	exit (1);
    	
    return (XV_OK);
}
