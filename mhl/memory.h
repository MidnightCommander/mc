#ifndef MHL_MEMORY_H
#define MHL_MEMORY_H

#include <memory.h>
#include <stdlib.h>

/* allocate a chunk of stack memory, uninitialized */
#define 	mhl_mem_alloc_u(sz)	(malloc(sz))

/* allocate a chunk of stack memory, zeroed */
#define		mhl_mem_alloc_z(sz)	(calloc(1,sz))

/* free a chunk of memory from stack, passing NULL does no harm */
static inline void g_free(void* ptr)
{
    if (ptr) free(ptr);
}

/* free an ptr and NULL it */
#define 	MHL_PTR_FREE(ptr)	do { g_free(ptr); (ptr) = NULL; } while (0)

/* allocate a chunk on stack - automatically free'd on function exit */
#define		mhl_stack_alloc(sz)	(alloca(sz))

/* re-alloc memory chunk */
#define		mhl_mem_realloc(ptr,sz)	(realloc(ptr,sz))

#endif
