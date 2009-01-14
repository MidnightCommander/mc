#ifndef __MHL_MEM
#define __MHL_MEM

#if defined _AIX
  #pragma alloca
#endif

#include <config.h>
#include <memory.h>
#include <stdlib.h>

#ifdef __GNUC__
#    ifndef alloca
#        define alloca __builtin_alloca
#    endif
#else
#    ifdef _MSC_VER
#        include <malloc.h>
#        define alloca _alloca
#    else
#        if HAVE_ALLOCA_H
#            include <alloca.h>
#        else
#            ifdef _AIX
#pragma alloca
#            else
#                ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#                endif
#            endif
#        endif
#    endif
#endif

#ifndef HAVE_ALLOCA
#    include <stdlib.h>
#    if !defined(STDC_HEADERS) && defined(HAVE_MALLOC_H)
#        include <malloc.h>
#    endif
#    define alloca malloc
#endif

#ifdef WITH_GLIB
#	include <glib.h>

#	if GLIB_MAJOR_VERSION >= 2
		/* allocate a chunk of stack memory, uninitialized */
#		define 	mhl_mem_alloc_u(sz)	(g_try_malloc(sz))

		/* allocate a chunk of stack memory, zeroed */
#		define		mhl_mem_alloc_z(sz)	(g_try_malloc0(sz))

		/* re-alloc memory chunk */
#		define		mhl_mem_realloc(ptr,sz)	(g_try_realloc(ptr,sz))

#	else

		/* allocate a chunk of stack memory, uninitialized */
#		define 	mhl_mem_alloc_u(sz)	(g_malloc(sz))

		/* allocate a chunk of stack memory, zeroed */
#		define		mhl_mem_alloc_z(sz)	(g_malloc0(sz))

		/* re-alloc memory chunk */
#		define		mhl_mem_realloc(ptr,sz)	(g_realloc(ptr,sz))

#	endif

	/* allocate a chunk on stack - automatically free'd on function exit */
#	define		mhl_stack_alloc(sz)	(g_alloca(sz))

#else

	/* allocate a chunk of stack memory, uninitialized */
#	define 	mhl_mem_alloc_u(sz)	(malloc(sz))

	/* allocate a chunk of stack memory, zeroed */
#	define		mhl_mem_alloc_z(sz)	(calloc(1,sz))

	/* re-alloc memory chunk */
#	define		mhl_mem_realloc(ptr,sz)	(realloc(ptr,sz))

	/* allocate a chunk on stack - automatically free'd on function exit */
#	define		mhl_stack_alloc(sz)	(alloca(sz))

#endif

/* free a chunk of memory from stack, passing NULL does no harm */
static inline void mhl_mem_free(void* ptr)
{
    if (ptr)
#ifdef WITH_GLIB
	g_free(ptr);
#else
	free(ptr);
#endif
}

/* free an ptr and NULL it */
#define 	MHL_PTR_FREE(ptr)	do { mhl_mem_free(ptr); (ptr) = NULL; } while (0); 

#endif // __MHL_MEM
