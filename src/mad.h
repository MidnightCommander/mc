#ifndef __MAD_H
#define __MAD_H

/* To prevent molesting these files with the malloc/calloc/free macros.  */
#include <stdlib.h>
#include <malloc.h> 

#ifdef HAVE_MAD
#   define INLINE 
#else
#   ifndef INLINE
#       define INLINE inline
#   endif
#endif

#ifdef HAVE_MAD
#include <stdarg.h>

/* The Memory Allocation Debugging system */

/* GNU headers define this as macros */
#ifdef malloc
#   undef malloc
#endif

#ifdef calloc
#   undef calloc
#endif

#define tempnam(x,y)	mad_tempnam (x, y)

#define malloc(x)	mad_alloc (x, __FILE__, __LINE__)
#define calloc(x, y)	mad_alloc ((x) * (y), __FILE__, __LINE__)
#define realloc(x, y)	mad_realloc (x, y, __FILE__, __LINE__)
#define xmalloc(x, y)	mad_alloc (x, __FILE__, __LINE__)
#undef strdup
#define strdup(x)	mad_strdup (x, __FILE__, __LINE__)
#define free(x)		mad_free (x, __FILE__, __LINE__)

/* This defenitions are grabbed from GLib.h */
#define g_new(type, count)	  \
      ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#define g_new0(type, count)	  \
      ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#define g_renew(type, mem, count)	  \
      ((type *) g_realloc (mem, (unsigned) sizeof (type) * (count)))

#define g_malloc(x)	mad_alloc (x, __FILE__, __LINE__)
#define g_malloc0(x)	mad_alloc0 (x, __FILE__, __LINE__)
#define g_calloc(x, y)	mad_alloc ((x) * (y), __FILE__, __LINE__)
#define g_realloc(x, y)	mad_realloc (x, y, __FILE__, __LINE__)
#define g_strdup(x)	mad_strdup (x, __FILE__, __LINE__)
#define g_free(x)	mad_free (x, __FILE__, __LINE__)
#define g_strconcat		mad_strconcat
#define g_strdup_printf		mad_strdup_printf
#define g_strdup_vprintf	mad_strdup_vprintf

void mad_init (void);
void mad_set_debug (const char *file);
void mad_check (char *file, int line);
void *mad_alloc (int size, char *file, int line);
void *mad_alloc0 (int size, char *file, int line);
void *mad_realloc (void *ptr, int newsize, char *file, int line);
char *mad_strdup (const char *s, char *file, int line);
void mad_free (void *ptr, char *file, int line);
void mad_finalize (char *file, int line);
char *mad_tempnam (char *s1, char *s2);
char *mad_strconcat (const char *first, ...);
char *mad_strdup_printf (const char *format, ...);
char *mad_strdup_vprintf (const char *format, va_list args);

#else

#define mad_init()
#define mad_finalize(x, y)
#define mad_check(file,line)

#endif /* HAVE_MAD */

#endif /* __MAD_H */
