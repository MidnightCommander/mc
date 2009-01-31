#ifndef __MHL_STRING_H
#define __MHL_STRING_H

#include <ctype.h>
#include <stdarg.h>
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

#define __STR_CONCAT_MAX	32
/* _NEVER_ call this function directly ! */
static inline char* __mhl_str_concat_hlp(const char* base, ...)
{
    static const char* arg_ptr[__STR_CONCAT_MAX];
    static size_t      arg_sz[__STR_CONCAT_MAX];
    int         count = 0;
    size_t      totalsize = 0;

    if (base)
    {
	arg_ptr[0] = base;
	arg_sz[0]  = totalsize = strlen(base);
	count = 1;
    }

    va_list args;
    va_start(args,base);
    char* a;
    /* note: we use ((char*)(1)) as terminator - NULL is a valid argument ! */
    while ((a = va_arg(args, char*))!=(char*)1)
    {
	if (a)
	{
	    arg_ptr[count] = a;
	    arg_sz[count]  = strlen(a);
	    totalsize += arg_sz[count];
	    count++;
	}
    }

    if (!count)
	return mhl_str_dup("");

    /* now as we know how much to copy, allocate the buffer */
    char* buffer = (char*)mhl_mem_alloc_u(totalsize+2);
    char* current = buffer;
    int x=0;
    for (x=0; x<count; x++)
    {
	memcpy(current, arg_ptr[x], arg_sz[x]);
	current += arg_sz[x];
    }

    *current = 0;
    return buffer;
}

#define mhl_str_concat(...)	(__mhl_str_concat_hlp(__VA_ARGS__, (char*)(1)))

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
    int dnlen = strlen(dirname);
    while (dnlen && (dirname[dnlen-1]=='/'))
	dnlen--;

    int fnlen = strlen(filename);
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
