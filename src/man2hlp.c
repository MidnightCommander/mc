/* Man page to help file converter
   Copyright (C) 1994, 1995 Janne Kukonlehto
   
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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "help.h"

#define BUFFER_SIZE 256

static char *filename;		/* The name of the input file */
static int width;		/* Output width in characters */
static int col = 0;		/* Current output column */
static int out_row = 1;		/* Current output row */
static int in_row = 0;		/* Current input row */
static int no_split_flag = 0;	/* Flag: Don't split section on next ".SH" */
static int skip_flag = 0;	/* Flag: Skip this section.
				   0 = don't skip,
				   1 = skipping title,
				   2 = title skipped, skipping text */
static int link_flag = 0;	/* Flag: Next line is a link */
static int verbatim_flag = 0;	/* Flag: Copy input to output verbatim */
static int node = 0;		/* Flag: This line is an original ".SH" */

/* Report error in input */
static void print_error (char *message)
{
    fprintf (stderr, "man2hlp: %s in file \"%s\" at row %d\n", message, filename, in_row);
}

/* Change output line */
static void newline (void)
{
    out_row ++;
    col = 0;
    printf("\n");
}

/* Calculate the length of string */
static int string_len (char *buffer)
{
    static int anchor_flag = 0;	/* Flag: Inside hypertext anchor name */
    static int link_flag = 0;	/* Flag: Inside hypertext link target name */
    int backslash_flag = 0;	/* Flag: Backslash quoting */
    int c;			/* Current character */
    int len = 0;		/* Result: the length of the string */

    while (*(buffer))
    {
	c = *buffer++;
	if (c == CHAR_LINK_POINTER) 
	    link_flag = 1;	/* Link target name starts */
	else if (c == CHAR_LINK_END)
	    link_flag = 0;	/* Link target name ends */
	else if (c == CHAR_NODE_END){
	    /* Node anchor name starts */
	    anchor_flag = 1;
	    /* Ugly hack to prevent loss of one space */
	    len ++;
	}
	/* Don't add control characters to the length */
	if (c >= 0 && c < 32)
	    continue;
	/* Attempt to handle backslash quoting */
	if (c == '\\' && !backslash_flag){
	    backslash_flag = 1;
	    continue;
	}
	backslash_flag = 0;
	/* Increase length if not inside anchor name or link target name */
	if (!anchor_flag && !link_flag)
	    len ++;
	if (anchor_flag && c == ']'){
	    /* Node anchor name ends */
	    anchor_flag = 0;
	}
    }
    return len;
}

/* Output the string */
static void print_string (char *buffer)
{
    int len;		/* The length of current word */
    int c;		/* Current character */
    int backslash_flag = 0;

    /* Skipping lines? */
    if (skip_flag)
	return;
    /* Copying verbatim? */
    if (verbatim_flag){
	/* Attempt to handle backslash quoting */
	while (*(buffer)){
	    c = *buffer++;
	    if (c == '\\' && !backslash_flag){
		backslash_flag = 1;
		continue;
	    }
	    backslash_flag = 0;
	    printf ("%c", c);
	}
    } else {
	/* Split into words */
	buffer = strtok (buffer, " \t\n");
	/* Repeat for each word */
	while (buffer){
	    /* Skip empty strings */  
	    if (*(buffer)){
		len = string_len (buffer);
		/* Change the line if about to break the right margin */
		if (col + len >= width)
		    newline ();
		/* Words are separated by spaces */
		if (col > 0){
		    printf (" ");
		    col ++;
		}
		/* Attempt to handle backslash quoting */
		while (*(buffer))
		{
		    c = *buffer++;
		    if (c == '\\' && !backslash_flag){
			backslash_flag = 1;
			continue;
		    }
		    backslash_flag = 0;
		    printf ("%c", c);
		}
		/* Increase column */
		col += len;
	    }
	    /* Get the next word */
	    buffer = strtok (NULL, " \t\n");
	} /* while */
    }
}

/* Like print_string but with printf-like syntax */
static void printf_string (char *format, ...)
{
    va_list args;
    char buffer [BUFFER_SIZE];

    va_start (args, format);
    vsprintf (buffer, format, args);
    va_end (args);
    print_string (buffer);
}

/* Handle all the roff dot commands */
static void handle_command (char *buffer)
{
    int len, heading_level;

    /* Get the command name */
    strtok (buffer, " \t");
    if ((strcmp (buffer, ".SH") == 0) || (strcmp (buffer, ".\\\"NODE") == 0)){
	int SH = (strcmp (buffer, ".SH") == 0);
	/* If we already skipped a section, don't skip another */
	if (skip_flag == 2){
	    skip_flag = 0;
	}
	/* Get the command parameters */
	buffer = strtok (NULL, "");
	if (buffer == NULL){
	    print_error ("Syntax error: .SH: no title");
	    return;
	} else {
	    /* Remove quotes */
	    if (buffer[0] == '"'){
		buffer ++;
		len = strlen (buffer);
		if (buffer[len-1] == '"'){
		    len --;
		    buffer[len] = 0;
		}
	    }
	    /* Calculate heading level */
	    heading_level = 0;
	    while (buffer [heading_level] == ' ')
		heading_level ++;
	    /* Heading level must be even */
	    if (heading_level & 1)
		print_error ("Syntax error: .SH: odd heading level");
	    if (no_split_flag){
		/* Don't start a new section */
		newline ();
		print_string (buffer);
		newline ();
		newline ();
		no_split_flag = 0;
	    }
	    else if (skip_flag){
		/* Skipping title and marking text for skipping */
		skip_flag = 2;
	    }
	    else {
		if (!SH || !node){
		/* Start a new section */
		    printf ("%c[%s]", CHAR_NODE_END, buffer);
		    col ++;
		    newline ();
		}
		if (SH){
		    print_string (buffer + heading_level);
		    newline ();
		    newline ();
		}
	    } /* Start new section */
	} /* Has parameters */
	node = !SH;
    } /* Command .SH */
    else if (strcmp (buffer, ".\\\"DONT_SPLIT\"") == 0){
	no_split_flag = 1;
    }
    else if (strcmp (buffer, ".\\\"SKIP_SECTION\"") == 0){
	skip_flag = 1;
    }
    else if (strcmp (buffer, ".\\\"LINK2\"") == 0){
	/* Next two input lines form a link */
	link_flag = 2;
    }
    else if (strcmp (buffer, ".PP") == 0){
	/* End of paragraph */
	if (col > 0) newline();
	newline ();
    }
    else if (strcmp (buffer, ".nf") == 0){
	/* Following input lines are to be handled verbatim */
	verbatim_flag = 1;
	if (col > 0) newline ();
    }
    else if (strcmp (buffer, ".I") == 0 || strcmp (buffer, ".B") == 0){
	/* Bold text or italics text */
	char type = buffer [1];
	char *p;
	char *w = buffer; 
	int backslash_flag = 0;

	buffer = strtok (NULL, "");
	if (buffer == NULL){
	    print_error ("Syntax error: .I / .B: no text");
	    return;
	}

	*w = (type == 'I') ? CHAR_ITALIC_ON : CHAR_BOLD_ON;

	/* Attempt to handle backslash quoting */
	for (p = buffer, buffer = w++; *p; p++){
	    if (*p == '\\' && !backslash_flag){
		backslash_flag = 1;
		continue;
	    }
	    backslash_flag = 0;
	    *w++ = *p;
	}

	*w++ = CHAR_BOLD_OFF;
	*w = 0;
	print_string (buffer);
    }
    else if (strcmp (buffer, ".TP") == 0){
	/* End of paragraph? */
	if (col > 0) newline ();
	newline ();
    }
    else if (strcmp (buffer, ".\\\"TOPICS") == 0){
	if (out_row > 1){
	    print_error ("Syntax error: .\\\"TOPICS must be first command");
	    return;
	}
	buffer = strtok (NULL, "");
	if (buffer == NULL){
	    print_error ("Syntax error: .\\\"TOPICS: no text");
	    return;
	}
	printf ("%s\n", buffer);
    }
    else {
	/* Other commands are ignored */
    }
}

static void handle_link (char *buffer)
{
    static char old [80];
    int len;

    switch (link_flag){
    case 1:
	/* Old format link, not supported */
	break;
    case 2:
	/* First part of new format link */
	strcpy (old, buffer);
	link_flag = 3;
	break;
    case 3:
	/* Second part of new format link */
	if (buffer [0] == '.')
	    buffer++;
	if (buffer [0] == '\\')
	    buffer++;
	if (buffer [0] == '"')
	    buffer++;
	len = strlen (buffer);
	if (len && buffer [len-1] == '"'){
	    buffer [--len] = 0;
	}
	printf_string ("%c%s%c%s%c\n", CHAR_LINK_START, old, CHAR_LINK_POINTER, buffer, CHAR_LINK_END);
	link_flag = 0;
	break;
    }
}

int main (int argc, char **argv)
{
    int len;			/* Length of input line */
    FILE *file;			/* Input file */
    char buffer2 [BUFFER_SIZE];	/* Temp input line */
    char *buffer = buffer2;	/* Input line */

    /* Validity check for arguments */
    if (argc != 3 || ((width = atoi (argv[1])) <= 10)){
	fprintf (stderr, "Usage: man2hlp <width> <file.man>\n");
	return 3;
    }

    /* Open the input file */
    filename = argv[2];
    file = fopen (filename, "r");
    if (file == NULL){
	sprintf (buffer, "man2hlp: Cannot open file \"%s\"", filename);
	perror (buffer);
	return 3;
    }

    /* Repeat for each input line */
    while (!feof (file)){
	/* Read a line */
	if (!fgets (buffer2, BUFFER_SIZE, file)){
	    break;
	}
	if (buffer2 [0] == '\\' && buffer2 [1] == '&')
	    buffer = buffer2 + 2;
	else
	    buffer = buffer2;
	in_row ++;
	len = strlen (buffer);
	/* Remove terminating newline */
	if (buffer [len-1] == '\n')
	{
	    len --;
	    buffer [len] = 0;
	}
	if (verbatim_flag){
	    /* Copy the line verbatim */
	    if (strcmp (buffer, ".fi") == 0){
		verbatim_flag = 0;
	    } else {
		print_string (buffer);
		newline ();
	    }
	}
	else if (link_flag)
	    /* The line is a link */
	    handle_link (buffer);
	else if (buffer[0] == '.')
	    /* The line is a roff command */
	    handle_command (buffer);
	else
	{
	    /* A normal line, just output it */
	    print_string (buffer);
	}
    }

    /* All done */
    newline ();
    fclose (file);
    return 0;
}
