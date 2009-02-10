#ifndef __MHL_STRING_H
#define __MHL_STRING_H

#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <mhl/memory.h>

#define	mhl_str_dup(str)	((str ? strdup(str) : strdup("")))
#define mhl_str_ndup(str,len)	((str ? strndup(str,len) : strdup("")))
#define mhl_str_len(str)	((str ? strlen(str) : 0))

static inline char * mhl_str_dup_range(const char * s_start, const char * s_bound)
{
    return mhl_str_ndup(s_start, s_bound - s_start);
}

static inline char* mhl_str_trim(char* str)
{
    if (!str) return NULL;	/* NULL string ?! bail out. */

    /* find the first non-space */
    char* start; for (start=str; ((*str) && (!isspace(*str))); str++);

    /* only spaces ? */
    if (!(*str)) { *str = 0; return str; }

    /* get the size (cannot be empty - catched above) */
    size_t _sz = strlen(str);

    /* find the proper end */
    char* end;
    for (end=(str+_sz-1); ((end>str) && (isspace(*end))); end--);
    end[1] = 0;		/* terminate, just to be sure */

    /* if we have no leading spaces, just trucate */
    if (start==str) { end++; *end = 0; return str; }

    /* if it' only one char, dont need memmove for that */
    if (start==end) { str[0]=*start; str[1]=0; return str; }

    /* by here we have a (non-empty) region between start end end */
    memmove(str,start,(end-start+1));
    return str;
}

static inline void mhl_str_toupper(char* str)
{
    if (str)
	for (;*str;str++)
	    *str = toupper(*str);
}

/* note: we use ((char*)(1)) as terminator - NULL is a valid argument ! */
static const char * mhl_s_c_sep__ = (const char *)1;
/* _NEVER_ call this function directly ! */
static inline char* mhl_str_concat_hlp__(const char* va_start_dummy, ...)
{
    char * result;
    size_t result_len = 0;
    char * p;
    const char * chunk;

    va_list args;
    va_start(args,va_start_dummy);
    while ((chunk = va_arg(args, const char*)) != mhl_s_c_sep__)
    {
	if (chunk)
	{
	    result_len += strlen (chunk);
	}
    }
    va_end(args);

    if (result_len == 0)
	return mhl_str_dup("");

    /* now as we know how much to copy, allocate the buffer + '\0'*/
    result = (char*)mhl_mem_alloc_u (result_len + 1);

    p = result;

    va_start(args,va_start_dummy);
    while ((chunk = va_arg(args, const char*)) != mhl_s_c_sep__)
    {
	if (chunk)
	{
	    size_t chunk_len = strlen (chunk);
	    memcpy (p, chunk, chunk_len);
	    p += chunk_len;
	}
    }
    va_end(args);

    *p = '\0';
    return result;
}

#define mhl_str_concat(...)	(mhl_str_concat_hlp__(mhl_s_c_sep__, __VA_ARGS__, mhl_s_c_sep__))

static inline char* mhl_str_reverse(char* ptr)
{
    if (!ptr) 	 		return NULL;	/* missing string */
    if (!(ptr[0] && ptr[1]))	return ptr;	/* empty or 1-ch string */

    size_t _sz = strlen(ptr);
    char* start = ptr;
    char* end   = ptr+_sz-1;

    while (start<end)
    {
	char c = *start;
	*start = *end;
	*end = c;
	start++;
	end--;
    }

    return ptr;
}

/*
 * strcpy is unsafe on overlapping memory areas, so define memmove-alike
 * string function. Has sense only when dest <= src.
 */
static inline char * mhl_strmove(char * dest, const char * src)
{
    size_t n = strlen (src) + 1; /* + '\0' */

    assert (dest<=src);

    return memmove(dest, src, n);
}

static inline char* mhl_str_dir_plus_file(const char* dirname, const char* filename)
{
    /* make sure we have valid strings */
    if (!dirname)
	dirname="";

    if (!filename)
	filename="";

    /* skip leading slashes on filename */
    while (*filename == '/')
	filename++;

    /* skip trailing slashes on dirname */
    size_t dnlen = strlen(dirname);
    while ((dnlen != 0) && (dirname[dnlen-1]=='/'))
	dnlen--;

    size_t fnlen = strlen(filename);
    char* buffer = mhl_mem_alloc_z(dnlen+fnlen+2);	/* enough space for dirname, /, filename, zero */
    char* ptr = buffer;

    memcpy(ptr, dirname, dnlen);
    ptr+=dnlen;
    *ptr = '/';
    ptr++;
    memcpy(ptr, filename, fnlen);
    ptr+=fnlen;
    *ptr = 0;

    return buffer;
}

#endif /* __MHL_STRING_H */
