#ifndef __MHL_STRING_H
#define __MHL_STRING_H

#include <ctype.h>
#include <stdarg.h>
#include "../mhl/memory.h"

#define	mhl_str_dup(str)	((str ? strdup(str) : strdup("")))
#define mhl_str_ndup(str,len)	((str ? strndup(str,len) : strdup("")))
#define mhl_str_len(str)	((str ? strlen(str) : 0))

inline char* mhl_str_trim(char*);

inline void mhl_str_toupper(char*);

#define __STR_CONCAT_MAX	32

/* _NEVER_ call this function directly ! */
inline char* __mhl_str_concat_hlp(const char*, ...);

#define mhl_str_concat(...)	(__mhl_str_concat_hlp(__VA_ARGS__, (char*)(1)))

inline char* mhl_str_reverse(char*);

#endif // __MHL_STRING_H
