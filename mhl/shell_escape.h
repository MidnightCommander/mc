#ifndef __MHL_SHELL_ESCAPE_H
#define __MHL_SHELL_ESCAPE_H

/* Micro helper library: shell escaping functions */

#include <string.h>
#include <stdlib.h>

#define mhl_shell_escape_toesc(x)	\
    (((x)==' ')||((x)=='!')||((x)=='#')||((x)=='$')||((x)=='%')||	\
     ((x)=='(')||((x)==')')||((x)=='\'')||((x)=='&')||((x)=='~')||	\
     ((x)=='{')||((x)=='}')||((x)=='[')||((x)==']')||((x)=='`')||	\
     ((x)=='?')||((x)=='|')||((x)=='<')||((x)=='>')||((x)==';')||	\
     ((x)=='*')||((x)=='\\')||((x)=='"'))

#define mhl_shell_escape_nottoesc(x)	\
    (((x)!=0) && (!mhl_shell_escape_toesc((x))))

static inline char* mhl_shell_escape_dup(const char* src)
{
    if ((src==NULL)||(!(*src)))
	return strdup("");

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
	    return buffer;

	*ptr = '\\';
	ptr++;
	*ptr = c;
	ptr++;
	src++;
    }
}

/* shell-unescape within a given buffer (writing to it!) */
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

#endif
