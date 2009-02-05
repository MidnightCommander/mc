/* Man page to help file converter
   Copyright (C) 1994, 1995, 1998, 2000, 2001, 2002, 2003, 2004, 2005,
   2007 Free Software Foundation, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/** \file man2hlp.c
 *  \brief Source: man page to help file converter
 */

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "help.h"
#include "glibcompat.h"

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

static int indentation;		/* Indentation level, n spaces */
static int tp_flag;		/* Flag: .TP paragraph
				   1 = this line is .TP label,
				   2 = first line of label description. */
static char *topics = NULL;

struct node {
    char *node;			/* Section name */
    char *lname;		/* Translated .SH, NULL if not translated */
    struct node *next;
    int heading_level;
};

static struct node nodes;
static struct node *cnode;	/* Current node */

#define MAX_STREAM_BLOCK 8192

/*
 * Read in blocks of reasonable size and make sure we read everything.
 * Failure to read everything is an error, indicated by returning 0.
 */
static size_t
persistent_fread (void *data, size_t len, FILE *stream)
{
    size_t count;
    size_t bytes_done = 0;
    char *ptr = (char *) data;

    if (len <= 0)
	return 0;

    while (bytes_done < len) {
	count = len - bytes_done;
	if (count > MAX_STREAM_BLOCK)
	    count = MAX_STREAM_BLOCK;

	count = fread (ptr, 1, count, stream);

	if (count <= 0)
	    return 0;

	bytes_done += count;
	ptr += count;
    }

    return bytes_done;
}

/*
 * Write in blocks of reasonable size and make sure we write everything.
 * Failure to write everything is an error, indicated by returning 0.
 */
static size_t
persistent_fwrite (const void *data, size_t len, FILE *stream)
{
    size_t count;
    size_t bytes_done = 0;
    const char *ptr = (const char *) data;

    if (len <= 0)
	return 0;

    while (bytes_done < len) {
	count = len - bytes_done;
	if (count > MAX_STREAM_BLOCK)
	    count = MAX_STREAM_BLOCK;

	count = fwrite (ptr, 1, count, stream);

	if (count <= 0)
	    return 0;

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
	g_snprintf (tmp, sizeof (tmp), "man2hlp: Cannot open file \"%s\"",
		    filename);
	perror (tmp);
	exit (3);
    }

    return f;
}

/* Do fclose(), exit if it fails */
static void
fclose_check (FILE *f)
{
    if (ferror (f)) {
	perror ("man2hlp: File error");
	exit (3);
    }

    if (fclose (f)) {
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
string_len (const char *buffer)
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
	    fputc (c, f_out);
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
		    fputc (' ', f_out);
		    col++;
		} else if (indentation) {
		    while (col++ < indentation)
			fputc (' ', f_out);
		}
		/* Attempt to handle backslash quoting */
		while (*(buffer)) {
		    c = *buffer++;
		    if (c == '\\' && !backslash_flag) {
			backslash_flag = 1;
			continue;
		    }
		    backslash_flag = 0;
		    fputc (c, f_out);
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
    g_vsnprintf (buffer, sizeof (buffer), format, args);
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
	    buffer += heading_level;
	    if (!is_sh || !node) {
		/* Start a new section, but omit empty section names */
		if (*buffer) {
		    fprintf (f_out, "%c[%s]", CHAR_NODE_END, buffer);
		    col++;
		    newline ();
		}

		/* Add section to the linked list */
		if (!cnode) {
		    cnode = &nodes;
		} else {
		    cnode->next = malloc (sizeof (nodes));
		    cnode = cnode->next;
		}
		cnode->node = strdup (buffer);
		cnode->lname = NULL;
		cnode->next = NULL;
		cnode->heading_level = heading_level;
	    }
	    if (is_sh) {
		/* print_string() strtok()es buffer, so */
		cnode->lname = strdup (buffer);
		print_string (buffer);
		newline ();
		newline ();
	    }
	}			/* Start new section */
    }				/* Has parameters */
    node = !is_sh;
}

/* Convert character from the macro name to the font marker */
static inline char
char_to_font (char c)
{
    switch (c) {
    case 'R':
	return CHAR_FONT_NORMAL;
    case 'B':
	return CHAR_FONT_BOLD;
    case 'I':
	return CHAR_FONT_ITALIC;
    default:
	return 0;
    }
}

/*
 * Handle alternate font commands (.BR, .IR, .RB, .RI, .BI, .IB)
 * Return 0 if the command wasn't recognized, 1 otherwise
 */
static int
handle_alt_font (char *buffer)
{
    char *p;
    char *w;
    char font[2];
    int in_quotes = 0;
    int alt_state = 0;

    if (strlen (buffer) != 3)
	return 0;

    if (buffer[0] != '.')
	return 0;

    font[0] = char_to_font (buffer[1]);
    font[1] = char_to_font (buffer[2]);

    /* Exclude names with unknown characters, .BB, .II and .RR */
    if (font[0] == 0 || font[1] == 0 || font[0] == font[1])
	return 0;

    p = strtok (NULL, "");
    if (p == NULL) {
	return 1;
    }

    w = buffer;
    *w++ = font[0];

    while (*p) {

	if (*p == '"') {
	    in_quotes = !in_quotes;
	    p++;
	    continue;
	}

	if (*p == ' ' && !in_quotes) {
	    p++;
	    /* Don't change font if we are at the end */
	    if (*p != 0) {
		alt_state = !alt_state;
		*w++ = font[alt_state];
	    }

	    /* Skip more spaces */
	    while (*p == ' ')
		p++;

	    continue;
	}

	*w++ = *p++;
    }

    /* Turn off attributes if necessary */
    if (font[alt_state] != CHAR_FONT_NORMAL)
	*w++ = CHAR_FONT_NORMAL;

    *w = 0;
    print_string (buffer);

    return 1;
}

/* Handle .IP and .TP commands.  is_tp is 1 for .TP, 0 for .IP */
static void
handle_tp_ip (int is_tp)
{
    if (col > 0)
	newline ();
    newline ();
    if (is_tp) {
	tp_flag = 1;
	indentation = 0;
    } else
	indentation = 8;
}

/* Handle all the roff dot commands.  See man groff_man for details */
static void
handle_command (char *buffer)
{
    int len;

    /* Get the command name */
    strtok (buffer, " \t");

    if (strcmp (buffer, ".SH") == 0) {
	indentation = 0;
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
    } else if ((strcmp (buffer, ".PP") == 0)
	       || (strcmp (buffer, ".P") == 0)
	       || (strcmp (buffer, ".LP") == 0)) {
	indentation = 0;
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
    } else if (strcmp (buffer, ".TP") == 0) {
	handle_tp_ip (1);
    } else if (strcmp (buffer, ".IP") == 0) {
	handle_tp_ip (0);
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
    } else if (handle_alt_font (buffer) == 1) {
	return;
    } else {
	/* Other commands are ignored */
	char warn_str[BUFFER_SIZE];
	g_snprintf (warn_str, sizeof (warn_str),
		    "Warning: unsupported command %s", buffer);
	print_error (warn_str);
	return;
    }
}

static struct links {
    char *linkname;		/* Section name */
    int line;			/* Input line in ... */
    const char *filename;
    struct links *next;
} links, *current_link;

static void
handle_link (char *buffer)
{
    static char old[80];
    int len;
    char *amp;
    const char *amp_arg;

    switch (link_flag) {
    case 1:
	/* Old format link, not supported */
	break;
    case 2:
	/* First part of new format link */
	/* Bold text or italics text */
	if (buffer[0] == '.' && (buffer[1] == 'I' || buffer[1] == 'B'))
	    for (buffer += 2; *buffer == ' ' || *buffer == '\t'; buffer++);
	g_strlcpy (old, buffer, sizeof (old));
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

	/* "Layout\&)," -- "Layout" should be highlighted, but not ")," */
	amp = strstr (old, "\\&");
	if (amp) {
	    *amp = 0;
	    amp += 2;
	    amp_arg = amp;
	} else {
	    amp_arg = "";
	}

	printf_string ("%c%s%c%s%c%s\n", CHAR_LINK_START, old,
		       CHAR_LINK_POINTER, buffer, CHAR_LINK_END, amp_arg);
	link_flag = 0;
	/* Add to the linked list */
	if (current_link) {
	    current_link->next = malloc (sizeof (links));
	    current_link = current_link->next;
	    current_link->next = NULL;
	} else {
	    current_link = &links;
	}
	current_link->linkname = strdup (buffer);
	current_link->filename = c_in;
	current_link->line = in_row;
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
    while (fgets (buffer, BUFFER_SIZE, f_man)) {
	char *input_line;	/* Input line without initial "\&" */

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
	} else if (link_flag) {
	    /* The line is a link */
	    handle_link (input_line);
	} else if (buffer[0] == '.') {
	    /* The line is a roff command */
	    handle_command (input_line);
	} else {
	    /* A normal line, just output it */
	    print_string (input_line);
	}
	/* .TP label processed as usual line */
	if (tp_flag) {
	    if (tp_flag == 1) {
		tp_flag = 2;
	    } else {
		tp_flag = 0;
		indentation = 8;
		if (col >= indentation)
		    newline ();
		else
		    while (++col < indentation)
			fputc (' ', f_out);
	    }
	}
    }

    newline ();
    fclose_check (f_man);
    /* First stage ends here, closing the manual */

    /* Second stage - process the template file */
    f_tmpl = fopen_check (c_tmpl, "r");
    c_in = c_tmpl;

    /* Repeat for each input line */
    /* Read a line */
    while (fgets (buffer, BUFFER_SIZE, f_tmpl)) {
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
		char *p = strchr (node, ']');
		if (p) {
		    if (strncmp (node + 1, "[main]", 6) == 0) {
			node = NULL;
		    } else {
			if (!cnode) {
			    cnode = &nodes;
			} else {
			    cnode->next = malloc (sizeof (nodes));
			    cnode = cnode->next;
			}
			cnode->node = strdup (node + 2);
			cnode->node[p - node - 2] = 0;
			cnode->lname = NULL;
			cnode->next = NULL;
			cnode->heading_level = 0;
		    }
		} else
		    node = NULL;
	    } else
		node = NULL;
	}
	fputs (buffer, f_out);
    }

    cont_start = ftell (f_out);
    if (cont_start <= 0) {
	perror (c_out);
	return 1;
    }

    if (topics)
	fprintf (f_out, "\004[Contents]\n%s\n\n", topics);
    else
	fprintf (f_out, "\004[Contents]\n");

    for (current_link = &links; current_link && current_link->linkname;) {
	int found = 0;
	struct links *next = current_link->next;

	if (strcmp (current_link->linkname, "Contents") == 0) {
	    found = 1;
	} else {
	    for (cnode = &nodes; cnode && cnode->node; cnode = cnode->next) {
		if (strcmp (cnode->node, current_link->linkname) == 0) {
		    found = 1;
		    break;
		}
	    }
	}
	if (!found) {
	    g_snprintf (buffer, sizeof (buffer), "Stale link \"%s\"",
			current_link->linkname);
	    c_in = current_link->filename;
	    in_row = current_link->line;
	    print_error (buffer);
	}
	free (current_link->linkname);
	if (current_link != &links)
	    free (current_link);
	current_link = next;
    }

    for (cnode = &nodes; cnode && cnode->node;) {
	char *node = cnode->node;
	struct node *next = cnode->next;

	if (*node)
	    fprintf (f_out, "  %*s\001%s\002%s\003", cnode->heading_level,
		     "", cnode->lname ? cnode->lname : node, node);
	fprintf (f_out, "\n");

	free (cnode->node);
	if (cnode->lname)
	    free (cnode->lname);
	if (cnode != &nodes)
	    free (cnode);
	cnode = next;
    }

    file_end = ftell (f_out);

    /* Sanity check */
    if ((file_end <= 0) || (file_end - cont_start <= 0)) {
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

    if (!persistent_fread (outfile_buffer, file_end, f_out)) {
	perror (c_out);
	return 1;
    }

    fclose_check (f_out);
    /* Now the output file is in the memory */

    /* Again open output file for writing */
    f_out = fopen_check (c_out, "w");

    /* Write part after the "Contents" node */
    if (!persistent_fwrite
	(outfile_buffer + cont_start, file_end - cont_start, f_out)) {
	perror (c_out);
	return 1;
    }

    /* Write part before the "Contents" node */
    if (!persistent_fwrite (outfile_buffer, cont_start, f_out)) {
	perror (c_out);
	return 1;
    }

    free (outfile_buffer);
    fclose_check (f_out);
    /* Closing everything */

    return 0;
}
