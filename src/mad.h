#ifndef __MAD_H
#define __MAD_H

/* To prevent molesting these files with the malloc/calloc/free macros.  */
#include <stdio.h>
#include <stdlib.h>
#if !defined(STDC_HEADERS) && defined(HAVE_MALLOC_H)
#   include <malloc.h>
#endif

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

#define tempnam(x,y)	mad_tempnam (x, y, __FILE__, __LINE__)

#define malloc(x)	mad_alloc (x, __FILE__, __LINE__)
#define calloc(x, y)	mad_alloc0 ((x) * (y), __FILE__, __LINE__)
#define realloc(x, y)	mad_realloc (x, y, __FILE__, __LINE__)
#define xmalloc(x, y)	mad_alloc (x, __FILE__, __LINE__)
#ifdef strdup
#   undef strdup
#endif
#define strdup(x)	mad_strdup (x, __FILE__, __LINE__)
#ifdef strndup
#   undef strndup
#endif
#define strndup(x, n)	mad_strndup (x, n, __FILE__, __LINE__)
#define free(x)		mad_free (x, __FILE__, __LINE__)

/*
 * Define MAD_GLIB to debug allocations in glib as well.
 * This code is not functional yet.
 */

#define MAD_GLIB
#ifdef MAD_GLIB
/* These definitions are grabbed from GLib.h */
#define g_new(type, count)	  \
      ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#define g_new0(type, count)	  \
      ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#define g_renew(type, mem, count)	  \
      ((type *) g_realloc (mem, (unsigned) sizeof (type) * (count)))

#define g_malloc(x)	mad_alloc (x, __FILE__, __LINE__)
#define g_malloc0(x)	mad_alloc0 (x, __FILE__, __LINE__)
#define g_realloc(x, y)	mad_realloc (x, y, __FILE__, __LINE__)
#define g_strdup(x)	mad_strdup (x, __FILE__, __LINE__)
#define g_strndup(x, n)	mad_strndup (x, n, __FILE__, __LINE__)
#define g_free(x)	mad_free (x, __FILE__, __LINE__)
#define g_strconcat		mad_strconcat
#define g_strdup_printf		mad_strdup_printf
#define g_strdup_vprintf	mad_strdup_vprintf
#define g_get_current_dir()	mad_get_current_dir (__FILE__, __LINE__)
#endif /* MAD_GLIB */

void mad_init (void);
void mad_set_debug (const char *file);
void mad_check (const char *file, int line);
void *mad_alloc (int size, const char *file, int line);
void *mad_alloc0 (int size, const char *file, int line);
void *mad_realloc (void *ptr, int newsize, const char *file, int line);
char *mad_strdup (const char *s, const char *file, int line);
char *mad_strndup (const char *s, int n, const char *file, int line);
void mad_free (void *ptr, const char *file, int line);
void mad_finalize (const char *file, int line);
char *mad_tempnam (char *s1, char *s2, const char *file, int line);
char *mad_strconcat (const char *first, ...);
char *mad_strdup_printf (const char *format, ...);
char *mad_strdup_vprintf (const char *format, va_list args);
char *mad_get_current_dir (const char *file, int line);

#else

#define mad_init()
#define mad_finalize(x, y)
#define mad_check(file,line)

#endif /* HAVE_MAD */

#endif /* __MAD_H */
