/* Man page to help file converter
   Copyright (C) 1994, 1995 Janne Kukonlehto
		 2002  Andrew V. Samoilov
		 2002  Pavel Roskin

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

static const char *c_out;	/* Output filename */
static FILE *f_out;		/* Output file */

static const char *c_in;	/* Current input filename */

static char *topics = NULL;

struct node {
    char *node;
    char *lname;
    struct node *next;
};

static struct node nodes;
static struct node *cnode;

#define MAX_STREAM_BLOCK 8192

/*
 * Read in blocks of reasonable size and make sure we read everything.
 * Failure to read everything is an error.
 */
static size_t
persistent_fread (void *data, size_t len, FILE * stream)
{
    size_t count;
    size_t bytes_done = 0;
    char *ptr = (char *) data;

    while (bytes_done < len) {
	count = len - bytes_done;
	if (count > MAX_STREAM_BLOCK)
	    count = MAX_STREAM_BLOCK;

	count = fread (ptr, 1, count, stream);

	if (count <= 0)
	    return -1;

	bytes_done += count;
	ptr += count;
    }

    return bytes_done;
}

/*
 * Write in blocks of reasonable size and make sure we write everything.
 * Failure to write everything is an error.
 */
static size_t
persistent_fwrite (const void *data, size_t len, FILE * stream)
{
    size_t count;
    size_t bytes_done = 0;
    const char *ptr = (const char *) data;

    while (bytes_done < len) {
	count = len - bytes_done;
	if (count > MAX_STREAM_BLOCK)
	    count = MAX_STREAM_BLOCK;

	count = fwrite (ptr, 1, count, stream);

	if (count <= 0)
	    return -1;

	bytes_done += count;
	ptr += count;
    }

    return bytes_done;
}

/* Report error in input */
static void
print_error (const char *message)
{
    fprintf (stderr, "man2hlp: %s in file \"%s\" on line %d\n", message,
	     c_in, in_row);
}

/* Do fopen(), exit if it fails */
static FILE *
fopen_check (const char *filename, const char *flags)
{
    char tmp[BUFFER_SIZE];
    FILE *f;

    f = fopen (filename, flags);
    if (f == NULL) {
	sprintf (tmp, "man2hlp: Cannot open file \"%s\"", filename);
	perror (tmp);
	exit (3);
    }

    return f;
}

/* Do fclose(), exit if it fails */
static void
fclose_check (FILE * f)
{
    int ret;

    ret = fclose (f);
    if (ret != 0) {
	perror ("man2hlp: Cannot close file");
	exit (3);
    }
}

/* Change output line */
static void
newline (void)
{
    out_row++;
    col = 0;
    fprintf (f_out, "\n");
}

/* Calculate the length of string */
static int
string_len (char *buffer)
{
    static int anchor_flag = 0;	/* Flag: Inside hypertext anchor name */
    static int link_flag = 0;	/* Flag: Inside hypertext link target name */
    int backslash_flag = 0;	/* Flag: Backslash quoting */
    int c;			/* Current character */
    int len = 0;		/* Result: the length of the string */

    while (*(buffer)) {
	c = *buffer++;
	if (c == CHAR_LINK_POINTER)
	    link_flag = 1;	/* Link target name starts */
	else if (c == CHAR_LINK_END)
	    link_flag = 0;	/* Link target name ends */
	else if (c == CHAR_NODE_END) {
	    /* Node anchor name starts */
	    anchor_flag = 1;
	    /* Ugly hack to prevent loss of one space */
	    len++;
	}
	/* Don't add control characters to the length */
	if (c >= 0 && c < 32)
	    continue;
	/* Attempt to handle backslash quoting */
	if (c == '\\' && !backslash_flag) {
	    backslash_flag = 1;
	    continue;
	}
	backslash_flag = 0;
	/* Increase length if not inside anchor name or link target name */
	if (!anchor_flag && !link_flag)
	    len++;
	if (anchor_flag && c == ']') {
	    /* Node anchor name ends */
	    anchor_flag = 0;
	}
    }
    return len;
}

/* Output the string */
static void
print_string (char *buffer)
{
    int len;			/* The length of current word */
    int c;			/* Current character */
    int backslash_flag = 0;

    /* Skipping lines? */
    if (skip_flag)
	return;
    /* Copying verbatim? */
    if (verbatim_flag) {
	/* Attempt to handle backslash quoting */
	while (*(buffer)) {
	    c = *buffer++;
	    if (c == '\\' && !backslash_flag) {
		backslash_flag = 1;
		continue;
	    }
	    backslash_flag = 0;
	    fprintf (f_out, "%c", c);
	}
    } else {
	/* Split into words */
	buffer = strtok (buffer, " \t\n");
	/* Repeat for each word */
	while (buffer) {
	    /* Skip empty strings */
	    if (*(buffer)) {
		len = string_len (buffer);
		/* Change the line if about to break the right margin */
		if (col + len >= HELP_TEXT_WIDTH)
		    newline ();
		/* Words are separated by spaces */
		if (col > 0) {
		    fprintf (f_out, " ");
		    col++;
		}
		/* Attempt to handle backslash quoting */
		while (*(buffer)) {
		    c = *buffer++;
		    if (c == '\\' && !backslash_flag) {
			backslash_flag = 1;
			continue;
		    }
		    backslash_flag = 0;
		    fprintf (f_out, "%c", c);
		}
		/* Increase column */
		col += len;
	    }
	    /* Get the next word */
	    buffer = strtok (NULL, " \t\n");
	}			/* while */
    }
}

/* Like print_string but with printf-like syntax */
static void
printf_string (const char *format, ...)
{
    va_list args;
    char buffer[BUFFER_SIZE];

    va_start (args, format);
    vsprintf (buffer, format, args);
    va_end (args);
    print_string (buffer);
}

/* Handle NODE and .SH commands.  is_sh is 1 for .SH, 0 for NODE */
static void
handle_node (char *buffer, int is_sh)
{
    int len, heading_level;

    /* If we already skipped a section, don't skip another */
    if (skip_flag == 2) {
	skip_flag = 0;
    }
    /* Get the command parameters */
    buffer = strtok (NULL, "");
    if (buffer == NULL) {
	print_error ("Syntax error: .SH: no title");
	return;
    } else {
	/* Remove quotes */
	if (buffer[0] == '"') {
	    buffer++;
	    len = strlen (buffer);
	    if (buffer[len - 1] == '"') {
		len--;
		buffer[len] = 0;
	    }
	}
	/* Calculate heading level */
	heading_level = 0;
	while (buffer[heading_level] == ' ')
	    heading_level++;
	/* Heading level must be even */
	if (heading_level & 1)
	    print_error ("Syntax error: .SH: odd heading level");
	if (no_split_flag) {
	    /* Don't start a new section */
	    newline ();
	    print_string (buffer);
	    newline ();
	    newline ();
	    no_split_flag = 0;
	} else if (skip_flag) {
	    /* Skipping title and marking text for skipping */
	    skip_flag = 2;
	} else {
	    if (!is_sh || !node) {
		/* Start a new section, but omit empty section names */
		if (*(buffer + heading_level)) {
			fprintf (f_out, "%c[%s]", CHAR_NODE_END,
				 buffer + heading_level);
			col++;
			newline ();
		}

		/* Add section to the linked list */
		if (!cnode) {
		    cnode = &nodes;
		    cnode->next = NULL;
		} else {
		    cnode->next = malloc (sizeof (nodes));
		    cnode = cnode->next;
		}
		cnode->node = strdup (buffer);
		cnode->lname = NULL;
	    }
	    if (is_sh) {
		/* print_string() strtok()es buffer, so */
		cnode->lname = strdup (buffer + heading_level);
		print_string (buffer + heading_level);
		newline ();
		newline ();
	    }
	}			/* Start new section */
    }				/* Has parameters */
    node = !is_sh;
}

/* Handle all the roff dot commands.  See man groff_man for details */
static void
handle_command (char *buffer)
{
    int len;

    /* Get the command name */
    strtok (buffer, " \t");

    if (strcmp (buffer, ".SH") == 0) {
	handle_node (buffer, 1);
    } else if (strcmp (buffer, ".\\\"NODE") == 0) {
	handle_node (buffer, 0);
    } else if (strcmp (buffer, ".\\\"DONT_SPLIT\"") == 0) {
	no_split_flag = 1;
    } else if (strcmp (buffer, ".\\\"SKIP_SECTION\"") == 0) {
	skip_flag = 1;
    } else if (strcmp (buffer, ".\\\"LINK2\"") == 0) {
	/* Next two input lines form a link */
	link_flag = 2;
    } else if ((strcmp (buffer, ".PP") == 0) || (strcmp (buffer, ".P") == 0)
	       || (strcmp (buffer, ".LP") == 0)) {
	/* End of paragraph */
	if (col > 0)
	    newline ();
	newline ();
    } else if (strcmp (buffer, ".nf") == 0) {
	/* Following input lines are to be handled verbatim */
	verbatim_flag = 1;
	if (col > 0)
	    newline ();
    } else if (strcmp (buffer, ".I") == 0 || strcmp (buffer, ".B") == 0
	       || strcmp (buffer, ".SB") == 0) {
	/* Bold text or italics text */
	char *p;
	char *w;
	int backslash_flag = 0;

	/* .SB [text]
	 * Causes the text on the same line or the text on the
	 * next  line  to  appear  in boldface font, one point
	 * size smaller than the default font.
	 */

	/* FIXME: text is optional, so there is no error */
	p = strtok (NULL, "");
	if (p == NULL) {
	    print_error ("Syntax error: .I | .B | .SB : no text");
	    return;
	}

	*buffer = (buffer[1] == 'I') ? CHAR_FONT_ITALIC : CHAR_FONT_BOLD;

	/* Attempt to handle backslash quoting */
	for (w = &buffer[1]; *p; p++) {
	    if (*p == '\\' && !backslash_flag) {
		backslash_flag = 1;
		continue;
	    }
	    backslash_flag = 0;
	    *w++ = *p;
	}

	*w++ = CHAR_FONT_NORMAL;
	*w = 0;
	print_string (buffer);
    } else if ((strcmp (buffer, ".TP") == 0)
	       || (strcmp (buffer, ".IP") == 0)) {
	/* TODO: Implement these indented paragraphs */
	if (col > 0)
	    newline ();
	newline ();
    } else if (strcmp (buffer, ".\\\"TOPICS") == 0) {
	if (out_row > 1) {
	    print_error
		("Syntax error: .\\\"TOPICS must be first command");
	    return;
	}
	buffer = strtok (NULL, "");
	if (buffer == NULL) {
	    print_error ("Syntax error: .\\\"TOPICS: no text");
	    return;
	}
	/* Remove quotes */
	if (buffer[0] == '"') {
	    buffer++;
	    len = strlen (buffer);
	    if (buffer[len - 1] == '"') {
		len--;
		buffer[len] = 0;
	    }
	}
	topics = strdup (buffer);
    } else if (strcmp (buffer, ".br") == 0) {
	if (col)
	    newline ();
    } else if (strncmp (buffer, ".\\\"", 3) == 0) {
	/* Comment */
    } else if (strcmp (buffer, ".TH") == 0) {
	/* Title header */
    } else if (strcmp (buffer, ".SM") == 0) {
	/* Causes the text on the same line or the text on the
	 * next  line  to  appear  in a font that is one point
	 * size smaller than the default font. */
	buffer = strtok (NULL, "");
	if (buffer)
	    print_string (buffer);
    } else {
	/* Other commands are ignored */
	/* There is no memmove on some systems */
	char warn_str[BUFFER_SIZE];
	strcpy (warn_str, "Warning: unsupported command ");
	strncat (warn_str, buffer, sizeof(warn_str));
	print_error (warn_str);
	return;
    }
}

static void
handle_link (char *buffer)
{
    static char old[80];
    int len;

    switch (link_flag) {
    case 1:
	/* Old format link, not supported */
	break;
    case 2:
	/* First part of new format link */
	/* Bold text or italics text */
	if (buffer[0] == '.' && (buffer[1] == 'I' || buffer[1] == 'B'))
	    for (buffer += 2; *buffer == ' ' || *buffer == '\t'; buffer++);
	strcpy (old, buffer);
	link_flag = 3;
	break;
    case 3:
	/* Second part of new format link */
	if (buffer[0] == '.')
	    buffer++;
	if (buffer[0] == '\\')
	    buffer++;
	if (buffer[0] == '"')
	    buffer++;
	len = strlen (buffer);
	if (len && buffer[len - 1] == '"') {
	    buffer[--len] = 0;
	}
	printf_string ("%c%s%c%s%c\n", CHAR_LINK_START, old,
		       CHAR_LINK_POINTER, buffer, CHAR_LINK_END);
	link_flag = 0;
	break;
    }
}

int
main (int argc, char **argv)
{
    int len;			/* Length of input line */
    const char *c_man;		/* Manual filename */
    const char *c_tmpl;		/* Template filename */
    FILE *f_man;		/* Manual file */
    FILE *f_tmpl;		/* Template file */
    char buffer[BUFFER_SIZE];	/* Full input line */
    char *node = NULL;
    char *outfile_buffer;	/* Large buffer to keep the output file */
    long cont_start;		/* Start of [Contents] */
    long file_end;		/* Length of the output file */

    /* Validity check for arguments */
    if (argc != 4) {
	fprintf (stderr,
		 "Usage: man2hlp file.man template_file helpfile\n");
	return 3;
    }

    c_man = argv[1];
    c_tmpl = argv[2];
    c_out = argv[3];

    /* First stage - process the manual, write to the output file */
    f_man = fopen_check (c_man, "r");
    f_out = fopen_check (c_out, "w");
    c_in = c_man;

    /* Repeat for each input line */
    while (!feof (f_man)) {
	char *input_line;	/* Input line without initial "\&" */

	/* Read a line */
	if (!fgets (buffer, BUFFER_SIZE, f_man)) {
	    break;
	}

	if (buffer[0] == '\\' && buffer[1] == '&')
	    input_line = buffer + 2;
	else
	    input_line = buffer;

	in_row++;
	len = strlen (input_line);
	/* Remove terminating newline */
	if (input_line[len - 1] == '\n') {
	    len--;
	    input_line[len] = 0;
	}

	if (verbatim_flag) {
	    /* Copy the line verbatim */
	    if (strcmp (input_line, ".fi") == 0) {
		verbatim_flag = 0;
	    } else {
		print_string (input_line);
		newline ();
	    }
	} else if (link_flag)
	    /* The line is a link */
	    handle_link (input_line);
	else if (buffer[0] == '.')
	    /* The line is a roff command */
	    handle_command (input_line);
	else {
	    /* A normal line, just output it */
	    print_string (input_line);
	}
    }

    newline ();
    fclose_check (f_man);
    /* First stage ends here, closing the manual */

    /* Second stage - process the template file */
    f_tmpl = fopen_check (c_tmpl, "r");
    c_in = c_tmpl;

    /* Repeat for each input line */
    while (!feof (f_tmpl)) {
	/* Read a line */
	if (!fgets (buffer, BUFFER_SIZE, f_tmpl)) {
	    break;
	}
	if (node) {
	    if (*buffer && *buffer != '\n') {
		cnode->lname = strdup (buffer);
		node = strchr (cnode->lname, '\n');
		if (node)
		    *node = 0;
	    }
	    node = NULL;
	} else {
	    node = strchr (buffer, CHAR_NODE_END);
	    if (node && (node[1] == '[')) {
		char *p = strrchr (node, ']');
		if (p && strncmp (node + 2, "main", 4) == 0
		    && node[6] == ']') {
		    node = 0;
		} else {
		    if (!cnode) {
			cnode = &nodes;
			cnode->next = NULL;
		    } else {
			cnode->next = malloc (sizeof (nodes));
			cnode = cnode->next;
		    }
		    cnode->node = strdup (node + 2);
		    cnode->node[p - node - 2] = 0;
		    cnode->lname = NULL;
		}
	    } else
		node = NULL;
	}
	fputs (buffer, f_out);
    }

    cont_start = ftell (f_out);
    if (topics)
	fprintf (f_out, "\004[Contents]\n%s\n\n", topics);
    else
	fprintf (f_out, "\004[Contents]\n");

    for (cnode = &nodes; cnode && cnode->node;) {
	char *node = cnode->node;
	int heading_level = 0;
	struct node *next = cnode->next;

	while (*node == ' ') {
	    heading_level++;
	    node++;
	}
	if (*node)
	    fprintf (f_out, "  %*s\001 %s \002%s\003", heading_level, "",
		     cnode->lname ? cnode->lname : node, node);
	fprintf (f_out, "\n");

	free (cnode->node);
	if (cnode->lname)
	    free (cnode->lname);
	if (cnode != &nodes)
	    free (cnode);
	cnode = next;
    }

    file_end = ftell (f_out);
    if (file_end <= 0) {
	perror (c_out);
	return 1;
    }

    fclose_check (f_out);
    fclose_check (f_tmpl);
    /* Second stage ends here, closing all files, note the end of output */

    /*
     * Third stage - swap two parts of the output file.
     * First, open the output file for reading and load it into the memory.
     */
    f_out = fopen_check (c_out, "r");

    outfile_buffer = malloc (file_end);
    if (!outfile_buffer)
	return 1;

    if (persistent_fread (outfile_buffer, file_end, f_out) < 0) {
	perror (c_out);
	return 1;
    }

    fclose_check (f_out);
    /* Now the output file is in the memory */

    /* Again open output file for writing */
    f_out = fopen_check (c_out, "w");

    /* Write part after the "Contents" node */
    if (persistent_fwrite
	(outfile_buffer + cont_start, file_end - cont_start, f_out) < 0) {
	perror (c_out);
	return 1;
    }

    /* Write part before the "Contents" node */
    if (persistent_fwrite (outfile_buffer, cont_start, f_out) < 0) {
	perror (c_out);
	return 1;
    }

    free (outfile_buffer);
    fclose_check (f_out);
    /* Closing everything */

    return 0;
}
