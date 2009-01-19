#ifndef __MHL_STRHASH_H
#define __MHL_STRHASH_H

#include <hash.h>
#include <mhl/memory.h>

static void __mhl_strhash_free_key(void* ptr)
{
    mhl_mem_free(ptr);
}

static void __mhl_strhash_free_dummy(void* ptr)
{
}

typedef	hash	MHL_STRHASH;

#define MHL_STRHASH_DECLARE(n)		MHL_STRHASH n;

#define MHL_STRHASH_INIT(h)			\
	hash_initialise(h, 997U, 		\
		hash_hash_string, 		\
		hash_compare_string, 		\
		hash_copy_string, 		\
		__mhl_strhash_free_key, 	\
		__mhl_strhash_free_dummy)

#define MHL_STRHASH_DECLARE_INIT(n)		\
	MHL_STRHASH_DECLARE(n);			\
	MHL_STRHASH_INIT(&n);

#define MHL_STRHASH_DEINIT(ht)			\
	hash_deinitialise(ht)

static inline void mhl_strhash_addkey(MHL_STRHASH* ht, const char* key, void* value)
{
    hash_insert(ht, (char*)key, value);
}

static inline void* mhl_strhash_lookup(MHL_STRHASH* ht, const char* key)
{
    void* retptr;
    if (hash_retrieve(ht, (char*)key, &retptr))
	return retptr;
    else
	return NULL;
}

#endif
