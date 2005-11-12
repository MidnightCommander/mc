#ifndef _JD_MACROS_H_
#define _JD_MACROS_H_

#ifndef SLMEMSET
# ifdef HAVE_MEMSET
#  define SLMEMSET memset
# else
#  define SLMEMSET SLmemset
# endif
#endif

#ifndef SLMEMCHR
# ifdef HAVE_MEMCHR
#  define SLMEMCHR memchr
# else
#  define SLMEMCHR SLmemchr
# endif
#endif

#ifndef SLMEMCPY
# ifdef HAVE_MEMCPY
#  define SLMEMCPY memcpy
# else
#  define SLMEMCPY SLmemcpy
# endif
#endif

/* Note:  HAVE_MEMCMP requires an unsigned memory comparison!!!  */
#ifndef SLMEMCMP
# ifdef HAVE_MEMCMP
#  define SLMEMCMP memcmp
# else
#  define SLMEMCMP SLmemcmp
# endif
#endif

#ifndef SLFREE
# define SLFREE free
#endif

#ifndef SLMALLOC
# define SLMALLOC malloc
#endif

#ifndef SLCALLOC
# define SLCALLOC calloc
#endif

#ifndef SLREALLOC
# define SLREALLOC realloc
#endif

#endif				       /* _JD_MACROS_H_ */
