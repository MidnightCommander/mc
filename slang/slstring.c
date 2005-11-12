/*
Copyright (C) 2004, 2005 John E. Davis

This file is part of the S-Lang Library.

The S-Lang Library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The S-Lang Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.  
*/

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

typedef struct _pSLstring_Type
{
   struct _pSLstring_Type *next;
   unsigned int ref_count;
   unsigned long hash;
   unsigned int len;
   char bytes [1];
}
SLstring_Type;

#define MAP_HASH_TO_INDEX(hash) ((hash) % SLSTRING_HASH_TABLE_SIZE)

static SLstring_Type *String_Hash_Table [SLSTRING_HASH_TABLE_SIZE];
static char Single_Char_Strings [256 * 2];

#if SLANG_OPTIMIZE_FOR_SPEED
#define MAX_FREE_STORE_LEN 32
static SLstring_Type *SLS_Free_Store [MAX_FREE_STORE_LEN];

# define NUM_CACHED_STRINGS 601
typedef struct 
{
   SLstring_Type *sls;
   char *str;
}
Cached_String_Type;
static char *Deleted_String = "*deleted*";
static Cached_String_Type Cached_Strings [NUM_CACHED_STRINGS];

#define GET_CACHED_STRING(s) \
   (Cached_Strings + (unsigned int)(((unsigned long) (s)) % NUM_CACHED_STRINGS))


_INLINE_
static void cache_string (SLstring_Type *sls)
{
   Cached_String_Type *cs;

   cs = GET_CACHED_STRING(sls->bytes);
   cs->str = sls->bytes;
   cs->sls = sls;
}

_INLINE_
static void uncache_string (char *s)
{
   Cached_String_Type *cs;
   
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	cs->sls = NULL;
	cs->str = Deleted_String;
     }
}
#endif

#if USE_NEW_HASH_CODE
/* This hash algorithm comes from:
 *
 *   Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net. 
 *   You may use this code any way you wish, private, educational, or commercial.  It's free.
 *   See http://burtleburtle.net/bob/hash/evahash.html
 */

typedef  unsigned long uint32;

#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

_INLINE_
unsigned long _pSLstring_hash (unsigned char *s, unsigned char *smax)
{
   register uint32 a,b,c;
   unsigned int length = (unsigned int)(smax - s);
   unsigned int len = length;
   
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = 0;

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
     {
	a += (s[0] +((uint32)s[1]<<8) +((uint32)s[2]<<16) +((uint32)s[3]<<24));
	b += (s[4] +((uint32)s[5]<<8) +((uint32)s[6]<<16) +((uint32)s[7]<<24));
	c += (s[8] +((uint32)s[9]<<8) +((uint32)s[10]<<16)+((uint32)s[11]<<24));
	mix(a,b,c);
	s += 12; len -= 12;
     }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((uint32)s[10]<<24);
   case 10: c+=((uint32)s[9]<<16);
   case 9 : c+=((uint32)s[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((uint32)s[7]<<24);
   case 7 : b+=((uint32)s[6]<<16);
   case 6 : b+=((uint32)s[5]<<8);
   case 5 : b+=s[4];
   case 4 : a+=((uint32)s[3]<<24);
   case 3 : a+=((uint32)s[2]<<16);
   case 2 : a+=((uint32)s[1]<<8);
   case 1 : a+=s[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return (unsigned long) c;
}
#else
_INLINE_
unsigned long _pSLstring_hash (unsigned char *s, unsigned char *smax)
{
   register unsigned long h = 0;
   register unsigned long sum = 0;
   unsigned char *smax4;

   smax4 = smax - 4;

   while (s < smax4)
     {
	sum += s[0];
	h = sum + (h << 1);
	sum += s[1];
	h = sum + (h << 1);
	sum += s[2];
	h = sum + (h << 1);
	sum += s[3];
	h = sum + (h << 1);
	
	s += 4;
     }

   while (s < smax)
     {
	sum += *s++;
	h ^= sum + (h << 3);	       /* slightly different */
     }

   return h;
}
#endif
unsigned long _pSLcompute_string_hash (char *s)
{
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;

   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     return cs->sls->hash;
#endif
   return _pSLstring_hash ((unsigned char *) s, (unsigned char *) s + strlen (s));
}

_INLINE_
/* This routine works with any (long) string */
static SLstring_Type *find_string (char *s, unsigned int len, unsigned long hash)
{
   SLstring_Type *sls;

   sls = String_Hash_Table [(unsigned int) MAP_HASH_TO_INDEX(hash)];

   if (sls == NULL)
     return NULL;

   do
     {
	/* Note that we need to actually make sure that bytes[len] == 0. 
	 * In this case, it is not enough to just compare pointers.  In fact,
	 * this is called from create_nstring, etc...  It is unlikely that the
	 * pointer is a slstring
	 */
	if ((sls->hash == hash)
	    && (sls->len == len)
	    && (0 == strncmp (s, sls->bytes, len)))
	  break;

	sls = sls->next;
     }
   while (sls != NULL);

   return sls;
}

_INLINE_
static SLstring_Type *find_slstring (char *s, unsigned long hash)
{
   SLstring_Type *sls;

   sls = String_Hash_Table [MAP_HASH_TO_INDEX(hash)];
   while (sls != NULL)
     {
	if (s == sls->bytes)
	  return sls;

	sls = sls->next;
     }
   return sls;
}

_INLINE_
static SLstring_Type *allocate_sls (unsigned int len)
{
   SLstring_Type *sls;
#if SLANG_OPTIMIZE_FOR_SPEED
   
   if ((len < MAX_FREE_STORE_LEN)
       && (NULL != (sls = SLS_Free_Store [len])))
     {
	SLS_Free_Store[len] = NULL;
	return sls;
     }
#endif
   /* FIXME: use structure padding */
   sls = (SLstring_Type *) SLmalloc (len + sizeof (SLstring_Type));
   if (sls != NULL)
     sls->len = len;
   return sls;
}

_INLINE_
static void free_sls (SLstring_Type *sls)
{
#if SLANG_OPTIMIZE_FOR_SPEED
   unsigned int len = sls->len;
   if ((len < MAX_FREE_STORE_LEN)
       && (SLS_Free_Store[len] == NULL))
     {
	SLS_Free_Store [len] = sls;
	return;
     }
#endif
   SLfree ((char *)sls);
}

_INLINE_
static char *create_long_string (char *s, unsigned int len, unsigned long hash)
{
   SLstring_Type *sls;

   sls = find_string (s, len, hash);

   if (sls != NULL)
     {
	sls->ref_count++;
#if SLANG_OPTIMIZE_FOR_SPEED
	cache_string (sls);
#endif
	return sls->bytes;
     }

   sls = allocate_sls (len);
   if (sls == NULL)
     return NULL;

   strncpy (sls->bytes, s, len);
   sls->bytes[len] = 0;
   sls->ref_count = 1;
   sls->hash = hash;
#if SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls);
#endif

   hash = MAP_HASH_TO_INDEX(hash);
   sls->next = String_Hash_Table [(unsigned int)hash];
   String_Hash_Table [(unsigned int)hash] = sls;

   return sls->bytes;
}

_INLINE_
static char *create_short_string (char *s, unsigned int len)
{
   char ch;

   /* Note: if len is 0, then it does not matter what *s is.  This is
    * important for SLang_create_nslstring.
    */
   if (len) ch = *s; else ch = 0;

   len = 2 * (unsigned int) ((unsigned char) ch);
   Single_Char_Strings [len] = ch;
   Single_Char_Strings [len + 1] = 0;
   return Single_Char_Strings + len;
}

/* s cannot be NULL */
_INLINE_
static SLstr_Type *create_nstring (char *s, unsigned int len, unsigned long *hash_ptr)
{
   unsigned long hash;

   if (len < 2)
     return create_short_string (s, len);

   hash = _pSLstring_hash ((unsigned char *) s, (unsigned char *) (s + len));
   *hash_ptr = hash;

   return create_long_string (s, len, hash);
}

SLstr_Type *SLang_create_nslstring (char *s, unsigned int len)
{
   unsigned long hash;
   if (s == NULL)
     return NULL;
   return create_nstring (s, len, &hash);
}

char *_pSLstring_make_hashed_string (char *s, unsigned int len, unsigned long *hashptr)
{
   unsigned long hash;

   if (s == NULL) return NULL;

   hash = _pSLstring_hash ((unsigned char *) s, (unsigned char *) s + len);
   *hashptr = hash;

   if (len < 2)
     return create_short_string (s, len);

   return create_long_string (s, len, hash);
}

char *_pSLstring_dup_hashed_string (char *s, unsigned long hash)
{
   unsigned int len;
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   
   if (s == NULL) return NULL;
   if (s[0] == 0)
     return create_short_string (s, 0);
   if (s[1] == 0)
     return create_short_string (s, 1);
     
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	cs->sls->ref_count += 1;
	return s;
     }
#else
   if (s == NULL) return NULL;
#endif

   len = strlen (s);
#if !SLANG_OPTIMIZE_FOR_SPEED
   if (len < 2) return create_short_string (s, len);
#endif

   return create_long_string (s, len, hash);
}

/* This function requires an slstring!!! */
char *_pSLstring_dup_slstring (char *s)
{
   SLstring_Type *sls;
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
#endif
   
   if (s == NULL)
     return s;
#if SLANG_OPTIMIZE_FOR_SPEED
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	cs->sls->ref_count += 1;
	return s;
     }
#endif
   if ((s[0] == 0) || (s[1] == 0))
     return s;
   
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   sls->ref_count++;
#if SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls);
#endif
   return s;
}

static void free_sls_string (SLstring_Type *sls)
{
   SLstring_Type *sls1, *prev;
   unsigned long hash = sls->hash;

   hash = MAP_HASH_TO_INDEX(hash);

   sls1 = String_Hash_Table [(unsigned int) hash];

   prev = NULL;

   /* This should not fail. */
   while (sls1 != sls)
     {
	prev = sls1;
	sls1 = sls1->next;
     }

   if (prev != NULL)
     prev->next = sls->next;
   else
     String_Hash_Table [(unsigned int) hash] = sls->next;

   free_sls (sls);
}

_INLINE_
static void free_long_string (char *s, unsigned long hash)
{
   SLstring_Type *sls;

   if (NULL == (sls = find_slstring (s, hash)))
     {
	SLang_verror (SL_APPLICATION_ERROR, "invalid attempt to free string:%s", s);
	return;
     }

   sls->ref_count--;
   if (sls->ref_count != 0)
     {
#if SLANG_OPTIMIZE_FOR_SPEED
	/* cache_string (sls, len, hash); */
#endif
	return;
     }
#if SLANG_OPTIMIZE_FOR_SPEED
   uncache_string (s);
#endif
   free_sls_string (sls);
}

/* This routine may be passed NULL-- it is not an error. */
void SLang_free_slstring (char *s)
{
   unsigned long hash;
   unsigned int len;
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
#endif

   if (s == NULL) return;

#if SLANG_OPTIMIZE_FOR_SPEED
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	SLstring_Type *sls = cs->sls;
	if (sls->ref_count <= 1)
	  {
#if SLANG_OPTIMIZE_FOR_SPEED
	     cs->sls = NULL;
	     cs->str = Deleted_String;
#endif
	     free_sls_string (sls);
	  }
	else
	  sls->ref_count -= 1;
	return;
     }
#endif

   if ((len = strlen (s)) < 2)
     return;

   hash = _pSLstring_hash ((unsigned char *)s, (unsigned char *) s + len);
   free_long_string (s, hash);
}

char *SLang_create_slstring (char *s)
{
   unsigned long hash;
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
#endif

   if (s == NULL) return NULL;
#if SLANG_OPTIMIZE_FOR_SPEED
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	cs->sls->ref_count += 1;
	return s;
     }
#endif

   return create_nstring (s, strlen (s), &hash);
}

void _pSLfree_hashed_string (char *s, unsigned int len, unsigned long hash)
{
   if ((s == NULL) || (len < 2)) return;
   free_long_string (s, hash);
}


char *_pSLallocate_slstring (unsigned int len)
{
   SLstring_Type *sls = allocate_sls (len);
   if (sls == NULL)
     return NULL;

   sls->hash = 0;
   return sls->bytes;
}

void _pSLunallocate_slstring (char *s, unsigned int len)
{
   SLstring_Type *sls;
   
   (void) len;
   if (s == NULL)
     return;
   
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   free_sls (sls);
}

char *_pSLcreate_via_alloced_slstring (char *s, unsigned int len)
{   
   unsigned long hash;
   SLstring_Type *sls;

   if (s == NULL)
     return NULL;
   
   if (len < 2)
     {
	char *s1 = create_short_string (s, len);
	_pSLunallocate_slstring (s, len);
	return s1;
     }

   /* s is not going to be in the cache because when it was malloced, its
    * value was unknown.  This simplifies the coding.
    */
   hash = _pSLstring_hash ((unsigned char *)s, (unsigned char *)s + len);
   sls = find_string (s, len, hash);
   if (sls != NULL)
     {
	sls->ref_count++;
	_pSLunallocate_slstring (s, len);
	s = sls->bytes;

#if SLANG_OPTIMIZE_FOR_SPEED
	cache_string (sls);
#endif
	return s;
     }
	
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   sls->ref_count = 1;
   sls->hash = hash;

#if SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls);
#endif

   hash = MAP_HASH_TO_INDEX(hash);
   sls->next = String_Hash_Table [(unsigned int)hash];
   String_Hash_Table [(unsigned int)hash] = sls;

   return s;
}

/* Note, a and b may be ordinary strings.  The result is an slstring */
char *SLang_concat_slstrings (char *a, char *b)
{
   unsigned int lena, len;
   char *c;

   lena = strlen (a);
   len = lena + strlen (b);

   c = _pSLallocate_slstring (len);
   if (c == NULL)
     return NULL;

   strcpy (c, a);
   strcpy (c + lena, b);

   return _pSLcreate_via_alloced_slstring (c, len);
}

/* This routine is assumed to work even if s is not an slstring */
unsigned int _pSLstring_bytelen (SLstr_Type *s)
{
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;

   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     return cs->sls->len;
#endif
   return strlen (s);
}

/* The caller must ensure that this is an slstring */
void _pSLang_free_slstring (SLstr_Type *s)
{
#if SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
#endif
   SLstring_Type *sls;

   if (s == NULL) return;

#if SLANG_OPTIMIZE_FOR_SPEED
   cs = GET_CACHED_STRING(s);
   if (cs->str == s)
     {
	sls = cs->sls;
	if (sls->ref_count <= 1)
	  {
#if SLANG_OPTIMIZE_FOR_SPEED
	     cs->sls = NULL;
	     cs->str = Deleted_String;
#endif
	     free_sls_string (sls);
	  }
	else
	  sls->ref_count -= 1;
	return;
     }
#endif
   
   if ((s[0] == 0) || (s[1] == 0))
     return;

   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   if (sls->ref_count > 1)
     {
	sls->ref_count--;
	return;
     }
   free_long_string (s, sls->hash);
}

/* An SLstring is required */
unsigned long _pSLstring_get_hash (SLstr_Type *s)
{
   SLstring_Type *sls;

   if (s[0] == 0)
     return _pSLstring_hash ((unsigned char*)s, (unsigned char *)s);
   if (s[1] == 0)
     return _pSLstring_hash ((unsigned char *)s, (unsigned char *)s+1);
   
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   return sls->hash;
}
