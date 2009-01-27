#ifndef __MHL_SHELL_ESCAPE_H
#define __MHL_SHELL_ESCAPE_H

/* Micro helper library: shell escaping functions */

#include <string.h>
#include <stdlib.h>

#include <mhl/types.h>

#define mhl_shell_escape_toesc(x)	\
    (((x)==' ')||((x)=='!')||((x)=='#')||((x)=='$')||((x)=='%')||	\
     ((x)=='(')||((x)==')')||((x)=='\'')||((x)=='&')||((x)=='~')||	\
     ((x)=='{')||((x)=='}')||((x)=='[')||((x)==']')||((x)=='`')||	\
     ((x)=='?')||((x)=='|')||((x)=='<')||((x)=='>')||((x)==';')||	\
     ((x)=='*')||((x)=='\\')||((x)=='"'))

#define mhl_shell_escape_nottoesc(x)	\
    (((x)!=0) && (!mhl_shell_escape_toesc((x))))

/* type for escaped string - just for a bit more type safety ;-p */
typedef struct { char* s; } SHELL_ESCAPED_STR;

/** To be compatible with the general posix command lines we have to escape
 strings for the command line

 /params const char * in
 string for escaping
 /returns
 return escaped string (later need to free)
 */
static inline SHELL_ESCAPED_STR mhl_shell_escape_dup(const char* src)
{
    if ((src==NULL)||(!(*src)))
	return (SHELL_ESCAPED_STR){ .s = strdup("") };

    char* buffer = calloc(1, strlen(src)*2+2);
    char* ptr = buffer;

    /* look for the first char to escape */
    while (1)
    {
	char c;
	/* copy over all chars not to escape */
	while ((c=(*src)) && mhl_shell_escape_nottoesc(c))
	{
	    *ptr = c;
	    ptr++;
	    src++;
	}

	/* at this point we either have an \0 or an char to escape */
	if (!c)
	    return (SHELL_ESCAPED_STR){ .s = buffer };

	*ptr = '\\';
	ptr++;
	*ptr = c;
	ptr++;
	src++;
    }
}

/** Unescape paths or other strings for e.g the internal cd
    shell-unescape within a given buffer (writing to it!)

 /params const char * in
 string for unescaping
 /returns
 return unescaped string
*/
static inline char* mhl_shell_unescape_buf(char* text)
{
    if (!text)
	return NULL;

    // look for the first \ - that's quick skipover if there's nothing to escape
    char* readptr = text;
    while ((*readptr) && ((*readptr)!='\\'))	readptr++;
    if (!(*readptr)) return text;

    // if we're here, we're standing on the first '\'
    char* writeptr = readptr;
    char c;
    while ((c = *readptr))
    {
	if (c=='\\')
	{
	    readptr++;
	    switch ((c = *readptr))
	    {
		case 'n':	(*writeptr) = '\n'; writeptr++;	break;
		case 'r':	(*writeptr) = '\r'; writeptr++;	break;
		case 't':	(*writeptr) = '\t'; writeptr++;	break;

		case ' ':
		case '\\':
		case '#':
		case '$':
		case '%':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '<':
		case '>':
		case '!':
		case '*':
		case '?':
		case '~':
		case '`':
		case '"':
		case ';':
		default:
		    (*writeptr) = c; writeptr++; break;
	    }
	}
	else	// got a normal character
	{
	    (*writeptr) = *readptr;
	    writeptr++;
	}
	readptr++;
    }
    *writeptr = 0;

    return text;
}

/** Check if char in pointer contain escape'd chars

 /params const char * in
 string for checking
 /returns
 return TRUE if string contain escaped chars
 otherwise return FALSE
 */
static inline bool
mhl_shell_is_char_escaped ( const char *in ) 
{
    if (in == NULL || !*in || in[0] != '\\') 
	return false;
    if (mhl_shell_escape_toesc(in[1]))
	return true;
    return false;
}

#endif
