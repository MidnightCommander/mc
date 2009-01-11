#ifndef __MHL_STRING_H
#define __MHL_STRING_H

#include <ctype.h>
#include <stdarg.h>
#include "../mhl/memory.h"

#define	mhl_str_dup(str)	((str ? strdup(str) : strdup("")))
#define mhl_str_ndup(str,len)	((str ? strndup(str,len) : strdup("")))
#define mhl_str_len(str)	((str ? strlen(str) : 0))

static inline char* mhl_str_trim(char* str)
{
    if (!str) return NULL;	// NULL string ?! bail out.

    // find the first non-space
    char* start; for (start=str; ((*str) && (!isspace(*str))); str++);

    // only spaces ?
    if (!(*str)) { *str = 0; return str; }

    // get the size (cannot be empty - catched above)
    int _sz = strlen(str);

    // find the proper end
    char* end;
    for (end=(str+_sz-1); ((end>str) && (isspace(*end))); end--);
    end[1] = 0;		// terminate, just to be sure

    // if we have no leading spaces, just trucate
    if (start==str) { end++; *end = 0; return str; }


    // if it' only one char, dont need memmove for that    
    if (start==end) { str[0]=*start; str[1]=0; return str; }

    // by here we have a (non-empty) region between start end end 
    memmove(str,start,(end-start+1));
    return str;
}

static inline void mhl_str_toupper(char* str)
{
    if (str)
	for (;*str;str++)
	    *str = toupper(*str);
}

static inline char* mhl_str_concat(int n, ...)
{
  va_list ap;
  char* buf = NULL;
  char* ptr = buf;

  va_start(ap,n);
  for (;n;n--) {
    char *foo = va_arg(ap, char*);
    int bar = strlen(foo);
    int length += bar;
    mhl_mem_realloc(buf, length);
    memcpy(buf,foo,bar);
    buf+bar;
  }
  va_end(ap);
  *buf = 0;
  return ptr;
}

/* concat 1 string to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_1(const char* base, const char* one)
{
    if (!base) return mhl_str_dup(one);
    if (!one)  return mhl_str_dup(base);

    int _sz_base = strlen(base);
    int _sz_one  = strlen(one);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+2);
    memcpy(buf, base, _sz_base);
    memcpy(buf+_sz_base, one, _sz_one);
    buf[_sz_base+_sz_one] = 0;
    return buf;
}

/* concat 2 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_2(const char* base, const char* one, const char* two)
{
    if (!base) base = "";
    if (!one)  one  = "";
    if (!two)  two  = "";

    int _sz_base = strlen(base);
    int _sz_one  = strlen(one);
    int _sz_two  = strlen(two);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    *ptr = 0;
    return buf;
}

/* concat 3 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_3(const char* base, const char* one, const char* two, const char* three)
{
    if (!base)  base = "";
    if (!one)   one  = "";
    if (!two)   two  = "";
    if (!three) three = "";

    int _sz_base  = strlen(base);
    int _sz_one   = strlen(one);
    int _sz_two   = strlen(two);
    int _sz_three = strlen(three);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+_sz_three+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    memcpy(ptr, three, _sz_three);	ptr+=_sz_three;
    *ptr = 0;
    return buf;
}

/* concat 4 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_4(const char* base, const char* one, const char* two, const char* three, const char* four)
{
    if (!base)  base  = "";
    if (!one)   one   = "";
    if (!two)   two   = "";
    if (!three) three = "";
    if (!four)  four  = "";

    int _sz_base  = strlen(base);
    int _sz_one   = strlen(one);
    int _sz_two   = strlen(two);
    int _sz_three = strlen(three);
    int _sz_four  = strlen(four);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+_sz_three+_sz_four+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    memcpy(ptr, three, _sz_three);	ptr+=_sz_three;
    memcpy(ptr, four,  _sz_four);	ptr+=_sz_four;
    *ptr = 0;
    return buf;
}

/* concat 5 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_5(const char* base, const char* one, const char* two, const char* three, const char* four, const char* five)
{
    if (!base)  base  = "";
    if (!one)   one   = "";
    if (!two)   two   = "";
    if (!three) three = "";
    if (!four)  four  = "";
    if (!five)  five  = "";

    int _sz_base  = strlen(base);
    int _sz_one   = strlen(one);
    int _sz_two   = strlen(two);
    int _sz_three = strlen(three);
    int _sz_four  = strlen(four);
    int _sz_five  = strlen(five);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+_sz_three+_sz_four+_sz_five+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    memcpy(ptr, three, _sz_three);	ptr+=_sz_three;
    memcpy(ptr, four,  _sz_four);	ptr+=_sz_four;
    memcpy(ptr, five,  _sz_five);	ptr+=_sz_five;
    *ptr = 0;
    return buf;
}

/* concat 6 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_6(const char* base, const char* one, const char* two, 
  const char* three, const char* four, const char* five, const char* six)
{
    if (!base)  base  = "";
    if (!one)   one   = "";
    if (!two)   two   = "";
    if (!three) three = "";
    if (!four)  four  = "";
    if (!five)  five  = "";
    if (!six)   six   = "";

    int _sz_base  = strlen(base);
    int _sz_one   = strlen(one);
    int _sz_two   = strlen(two);
    int _sz_three = strlen(three);
    int _sz_four  = strlen(four);
    int _sz_five  = strlen(five);
    int _sz_six   = strlen(six);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+_sz_three+_sz_four+_sz_five+_sz_six+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    memcpy(ptr, three, _sz_three);	ptr+=_sz_three;
    memcpy(ptr, four,  _sz_four);	ptr+=_sz_four;
    memcpy(ptr, five,  _sz_five);	ptr+=_sz_five;
    memcpy(ptr, six,   _sz_six);	ptr+=_sz_six;
    *ptr = 0;
    return buf;
}

/* concat 7 strings to another and return as mhl_mem_alloc()'ed string */
static inline char* mhl_str_concat_7(const char* base, const char* one, const char* two, 
  const char* three, const char* four, const char* five, const char* six, const char* seven)
{
    if (!base)  base  = "";
    if (!one)   one   = "";
    if (!two)   two   = "";
    if (!three) three = "";
    if (!four)  four  = "";
    if (!five)  five  = "";
    if (!six)   six   = "";
    if (!seven) seven = "";

    int _sz_base  = strlen(base);
    int _sz_one   = strlen(one);
    int _sz_two   = strlen(two);
    int _sz_three = strlen(three);
    int _sz_four  = strlen(four);
    int _sz_five  = strlen(five);
    int _sz_six   = strlen(six);
    int _sz_seven = strlen(seven);

    char* buf = mhl_mem_alloc_u(_sz_base+_sz_one+_sz_two+_sz_three+_sz_four+_sz_five+_sz_six+_sz_seven+2);
    char* ptr = buf;
    memcpy(ptr, base,  _sz_base);	ptr+=_sz_base;
    memcpy(ptr, one,   _sz_one);	ptr+=_sz_one;
    memcpy(ptr, two,   _sz_two);	ptr+=_sz_two;
    memcpy(ptr, three, _sz_three);	ptr+=_sz_three;
    memcpy(ptr, four,  _sz_four);	ptr+=_sz_four;
    memcpy(ptr, five,  _sz_five);	ptr+=_sz_five;
    memcpy(ptr, six,   _sz_six);	ptr+=_sz_six;
    memcpy(ptr, seven, _sz_seven);	ptr+=_sz_seven;
    *ptr = 0;
    return buf;
}

static inline char* mhl_str_reverse(char* ptr)
{
    if (!ptr) 	 		return NULL;	// missing string
    if (!(ptr[0] && ptr[1]))	return ptr;	// empty or 1-ch string

    int _sz = strlen(ptr);
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

#endif
