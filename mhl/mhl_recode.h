#ifndef __MHL_RECODE_H
#define __MHL_RECODE_H

#ifdef WITH_GLIB
#	include <glib.h>
typedef GIConv mhl_iconv;
#else
#	include <iconv.h>
typedef iconv_t mhl_iconv;
#endif
#include "mhl/memory.h"

/**
  List of charsets types

  \todo support for multibyte charsets (except utf-8)
*/
enum mhl_charset_types
{
    /** multibyte chatsets. Ex: utf-16, KANJI, ...
        Reserved for future use */
    MHL_CHARSET_MULTIBYTE,

    /** UTF-8 */
    MHL_CHARSET_UTF8,

    /** 8-bit charset */
    MHL_CHARSET_8BIT,

    /** others handle as 7-bit charsets */
    MHL_CHARSET_7BIT
};

struct mhl_charset
{
    const char *name;
    int type;
};

void mhl_recode_init (void);

void mhl_recode_charset_init (struct mhl_charset *);


#define mhl_recode_charset_new() mhl_mem_alloc_z ( sizeof ( struct mhl_charset ) )
#define mhl_recode_charset_stack_new() mhl_stack_alloc ( sizeof ( struct mhl_charset ) )
#define mhl_recode_charset_delete(sz) mhl_mem_free ( sz )


#endif // __MHL_RECODE_H
