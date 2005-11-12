/* sllimits.h */
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

#define USE_NEW_HASH_CODE	1

#define SLSTRING_HASH_TABLE_SIZE	10007
#if 0
/* slstring.c: Size of the hash table used for strings (prime numbers) */
#ifdef __MSDOS_16BIT__
# define SLSTRING_HASH_TABLE_SIZE	601
#else
# define SLSTRING_HASH_TABLE_SIZE	6007 /* 2909 */
#endif
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
#if USE_NEW_HASH_CODE
# define SLGLOBALS_HASH_TABLE_SIZE	2048
# define SLLOCALS_HASH_TABLE_SIZE	64
# define SLSTATIC_HASH_TABLE_SIZE	64
#else
# define SLGLOBALS_HASH_TABLE_SIZE	2909
# define SLLOCALS_HASH_TABLE_SIZE	73
# define SLSTATIC_HASH_TABLE_SIZE	73
#endif

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

