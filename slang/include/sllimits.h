/* Copyright (c) 1998, 1999, 2001, 2002, 2003 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
/* sllimits.h */

/* slstring.c: Size of the hash table used for strings (prime numbers) */
#ifdef __MSDOS_16BIT__
# define SLSTRING_HASH_TABLE_SIZE	601
# define SLASSOC_HASH_TABLE_SIZE	601
#else
# define SLSTRING_HASH_TABLE_SIZE	2909
# define SLASSOC_HASH_TABLE_SIZE 	2909
#endif

/* slang.c: maximum size of run time stack */
#ifdef __MSDOS_16BIT__
# define SLANG_MAX_STACK_LEN		500
#else
# define SLANG_MAX_STACK_LEN		2500
#endif

/* slang.c: This sets the size on the depth of function calls */
#ifdef __MSDOS_16BIT__
# define SLANG_MAX_RECURSIVE_DEPTH	50
#else
# define SLANG_MAX_RECURSIVE_DEPTH	2500
#endif

/* slang.c: Size of the stack used for local variables */
#ifdef __MSDOS_16BIT__
# define SLANG_MAX_LOCAL_STACK		200
#else
# define SLANG_MAX_LOCAL_STACK		4096
#endif

/* slang.c: The size of the hash table used for local and global objects.
 * These should be prime numbers.
 */
#define SLGLOBALS_HASH_TABLE_SIZE	2909
#define SLLOCALS_HASH_TABLE_SIZE	73
#define SLSTATIC_HASH_TABLE_SIZE	73

/* Size of the keyboard buffer use by the ungetkey routines */
#ifdef __MSDOS_16BIT__
# define SL_MAX_INPUT_BUFFER_LEN	40
#else
# define SL_MAX_INPUT_BUFFER_LEN	1024
#endif

/* Maximum number of nested switch statements */
#define SLANG_MAX_NESTED_SWITCH		10

/* Size of the block stack (used in byte-compiling) */
#define SLANG_MAX_BLOCK_STACK_LEN	50

/* slfile.c: Max number of open file pointers */
#ifdef __MSDOS_16BIT__
# define SL_MAX_FILES			32
#else
# define SL_MAX_FILES			256
#endif

#if !defined(__MSDOS_16BIT__)
# define SLTT_MAX_SCREEN_COLS 512
# define SLTT_MAX_SCREEN_ROWS 512
#else
# define SLTT_MAX_SCREEN_ROWS 64
# define SLTT_MAX_SCREEN_COLS 75
#endif

