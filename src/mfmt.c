/* mfmt: sets bold and underline for mail files
   (c) 1995 miguel de icaza

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file mfmt.c
 *  \brief Source: sets bold and underline for mail files
 */

#include <stdio.h>

enum states {
    header,
    definition,
    plain,
    newline,
    seen_f,
    seen_r,
    seen_o,
    header_new,
    seen_m
};

int
main (void)
{
    int c;
    int state = newline;
    int space_seen = 0;
    
    while ((c = getchar ()) != EOF){
	switch (state){
	case plain:
	    if (c == '\n')
		state = newline;
	    putchar (c);
	    break;
	    
	case newline:
	    if (c == 'F')
		state = seen_f;
            else if (c == '\n')
                putchar ('\n');
 	    else {
		state = plain;
		putchar (c);
	    }
	    break;
	    
	case seen_f:
	    if (c == 'r')
		state = seen_r;
	    else {
		printf ("F%c", c);
		state = plain;
	    }
	    break;

	case seen_r:
	    if (c == 'o')
		state = seen_o;
	    else {
		state = plain;
		printf ("Fr%c", c);
	    } 
	    break;

	case seen_o:
	    if (c == 'm'){
		state = seen_m;
	    } else {
		state = plain;
		printf ("Fro%c", c);
	    }
	    break;

	case seen_m:
	    if (c == ' '){
		state = definition;
		printf ("_\bF_\br_\bo_\bm ");
	    } else {
		state = plain;
		printf ("From%c", c);
	    }
	    break;
		
	case header_new:
	    space_seen = 0;
            if (c == ' ' || c == '\t') {
                state = definition;
                putchar (c);
                break;
            }
	    if (c == '\n'){
		state = plain;
		putchar (c);
		break;
	    }
	    
	case header:
	    if (c == '\n'){
		putchar (c);
		state = header_new;
		break;
	    }
	    printf ("_\b%c", c);
	    if (c == ' ')
		space_seen = 1;
	    if (c == ':' && !space_seen)
		state = definition;
	    break;

	case definition:
	    if (c == '\n'){
		putchar (c);
		state = header_new;
		break;
	    }
	    printf ("%c\b%c", c, c);
	    break;
	}
    }
    return (0);
}
