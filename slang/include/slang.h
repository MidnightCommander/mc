#ifndef DAVIS_SLANG_H_
#define DAVIS_SLANG_H_
/* -*- mode: C; mode: fold; -*- */
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

#define SLANG_VERSION 20005
#define SLANG_VERSION_STRING "2.0.5"
/* #ifdef __DATE__ */
/* # define SLANG_VERSION_STRING SLANG_VERSION_STRING0 " " __DATE__ */
/* #else */
/* # define SLANG_VERSION_STRING SLANG_VERSION_STRING0 */
/* #endif */
/*{{{ System Dependent Macros and Typedefs */

#if defined(__WATCOMC__) && defined(DOS)
# ifndef __MSDOS__
#  define __MSDOS__
# endif
# ifndef  DOS386
#  define  DOS386
# endif
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif /* __watcomc__ */

#if defined(unix) || defined(__unix) || defined (_AIX) || defined(__NetBSD__) || defined(__MACH__)
# ifndef __unix__
#  define __unix__ 1
# endif
#endif

#if !defined(__GO32__)
# ifdef __unix__
#  define REAL_UNIX_SYSTEM
# endif
#endif

/* Set of the various defines for pc systems.  This includes OS/2 */
#ifdef __GO32__
# ifndef __DJGPP__
#  define __DJGPP__ 1
# endif
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
#endif

#ifdef __BORLANDC__
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#ifdef __MSDOS__
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
#endif

#if defined(OS2) || defined(__os2__)
# ifndef IBMPC_SYSTEM
#   define IBMPC_SYSTEM
# endif
# ifndef __os2__
#  define __os2__
# endif
#endif

#if defined(__NT__) || defined(__MINGW32__) /* || defined(__CYGWIN32__) */
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
#endif

#if defined(WIN32) || defined(__WIN32__)
# ifndef IBMPC_SYSTEM
#  define IBMPC_SYSTEM
# endif
# ifndef __WIN32__
#  define __WIN32__
# endif
#endif

#if defined(IBMPC_SYSTEM) || defined(VMS)
# ifdef REAL_UNIX_SYSTEM
#  undef REAL_UNIX_SYSTEM
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <stdio.h>
#include <stdarg.h>
#if defined(__STDC__) || defined(__BORLANDC__) || defined(__cplusplus)
# include <stddef.h>		       /* for offsetof */
#endif

#ifdef SIZEOF_SHORT
# define SLANG_SIZEOF_SHORT SIZEOF_SHORT
#endif
#ifdef SIZEOF_INT
# define SLANG_SIZEOF_INT SIZEOF_INT
#endif
#ifdef SIZEOF_FLOAT
# define SLANG_SIZEOF_FLOAT SIZEOF_FLOAT
#endif
#ifdef SIZEOF_DOUBLE
# define SLANG_SIZEOF_DOUBLE SIZEOF_DOUBLE
#endif

#if !defined(SIZEOF_SHORT) || !defined(SIZEOF_INT) || !defined(SIZEOF_LONG) || !defined(SIZEOF_FLOAT) || !defined(SIZEOF_DOUBLE)
# include <limits.h>
# if !defined(SIZEOF_SHORT) && defined(SHRT_MAX)
#  if SHRT_MAX == 32767
#   define SLANG_SIZEOF_SHORT 2
#  else
#   if SHRT_MAX == 2147483647L
#    define SLANG_SIZEOF_SHORT 4
#   endif
#  endif
# endif
# if !defined(SIZEOF_INT) && defined(INT_MAX)
#  if INT_MAX == 32767
#   define SLANG_SIZEOF_INT 2
#  else
#   if INT_MAX == 2147483647L
#    define SLANG_SIZEOF_INT 4
#   endif
#  endif
# endif
# if !defined(SIZEOF_LONG) && defined(LONG_MAX)
#  if LONG_MAX == 32767
#   define SLANG_SIZEOF_LONG 2
#  else
#   if LONG_MAX == 2147483647L
#    define SLANG_SIZEOF_LONG 4
#   else
#    define SLANG_SIZEOF_LONG 8
#   endif
#  endif
# endif
#endif

#ifndef SLANG_SIZEOF_SHORT
# define SLANG_SIZEOF_SHORT 2
#endif
#ifndef SLANG_SIZEOF_INT
# define SLANG_SIZEOF_INT 4
#endif
#ifndef SLANG_SIZEOF_LONG
# define SLANG_SIZEOF_LONG 4
#endif
#ifndef SLANG_SIZEOF_FLOAT
# define SLANG_SIZEOF_FLOAT 4
#endif
#ifndef SLANG_SIZEOF_DOUBLE
# define SLANG_SIZEOF_DOUBLE 8
#endif

/* ---------------------------- Generic Macros ----------------------------- */

/*  __SC__ is defined for Symantec C++
   DOS386 is defined for -mx memory model, 32 bit DOS extender. */

#if defined(__SC__) && !defined(DOS386)
# include <dos.h>
#endif

#if defined(__BORLANDC__)
# include <alloc.h>
#endif

#ifdef __GNUC__
# define SLATTRIBUTE_(x) __attribute__ (x)
#else
# define SLATTRIBUTE_(x)
#endif
#define SLATTRIBUTE_PRINTF(a,b) SLATTRIBUTE_((format(printf,a,b)))
  
#if defined (__cplusplus) || defined(__STDC__) || defined(IBMPC_SYSTEM)
typedef void *VOID_STAR;
#define SLCONST const
#else
typedef unsigned char *VOID_STAR;
#define SLCONST
#endif

typedef int (*FVOID_STAR)(void);

#if defined(__MSDOS__) && defined(__BORLANDC__)
# define SLFREE(buf)  farfree((void far *)(buf))
# define SLMALLOC(x) farmalloc((unsigned long) (x))
# define SLREALLOC(buf, n) farrealloc((void far *) (buf), (unsigned long) (n))
# define SLCALLOC(n, m) farcalloc((unsigned long) (n), (unsigned long) (m))
#else
# if defined(VMS) && !defined(__DECC)
#  define SLFREE VAXC$FREE_OPT
#  define SLMALLOC VAXC$MALLOC_OPT
#  define SLREALLOC VAXC$REALLOC_OPT
#  define SLCALLOC VAXC$CALLOC_OPT
# else
#  define SLFREE(x) free((char *)(x))
#  define SLMALLOC malloc
#  define SLREALLOC realloc
#  define SLCALLOC calloc
# endif
#endif

#if defined(__WIN32__) && SLANG_DLL
# define SL_EXPORT __declspec(dllexport)
# define SL_IMPORT __declspec(dllimport)
#else
# define SL_EXPORT
# define SL_IMPORT
#endif
#ifdef SLANG_SOURCE_
# define SL_EXTERN extern SL_EXPORT
#else
# define SL_EXTERN extern SL_IMPORT
#endif

SL_EXTERN char *SLdebug_malloc (unsigned long);
SL_EXTERN char *SLdebug_calloc (unsigned long, unsigned long);
SL_EXTERN char *SLdebug_realloc (char *, unsigned long);
SL_EXTERN void SLdebug_free (char *);
SL_EXTERN void SLmalloc_dump_statistics (void);
SL_EXTERN char *SLstrcpy(register char *, register char *);
SL_EXTERN int SLstrcmp(register char *, register char *);
SL_EXTERN char *SLstrncpy(char *, register char *, register  int);

/* GNU Midnight Commander uses replacements from glib */
#define MIDNIGHT_COMMANDER_CODE 1
#ifndef MIDNIGHT_COMMANDER_CODE

SL_EXTERN void SLmemset (char *, char, int);
SL_EXTERN char *SLmemchr (register char *, register char, register int);
SL_EXTERN char *SLmemcpy (char *, char *, int);
SL_EXTERN int SLmemcmp (char *, char *, int);

#endif                                /* !MIDNIGHT_COMMANDER_CODE */

/*}}}*/

/* SLstrings */
typedef char SLstr_Type;
/* An SLstr_Type object must be treated as a constant and may only be freed
 * by the SLang_free_slstring function and nothing else.
 */
SL_EXTERN SLstr_Type *SLang_create_nslstring (char *, unsigned int);
SL_EXTERN SLstr_Type *SLang_create_slstring (char *);
SL_EXTERN void SLang_free_slstring (SLstr_Type *);    /* handles NULL */
SL_EXTERN int SLang_pop_slstring (SLstr_Type **);   /* free with SLang_free_slstring */
SL_EXTERN SLstr_Type *SLang_concat_slstrings (SLstr_Type *a, SLstr_Type *b);

SL_EXTERN void SLstring_dump_stats (void);


/*{{{ UTF-8 and Wide Char support */

#if SLANG_SIZEOF_INT == 4
typedef unsigned int SLwchar_Type;
# define SLANG_WCHAR_TYPE SLANG_UINT_TYPE
#else
typedef unsigned long SLwchar_Type;
# define SLANG_WCHAR_TYPE SLANG_ULONG_TYPE
#endif
typedef unsigned char SLuchar_Type;

/* Maximum multi-byte len for a unicode wchar */
#define SLUTF8_MAX_MBLEN	6

/* If argument is 1, force UTF-8 mode on.  If argument is 0, force mode off.
 * If -1, determine mode from the locale.
 * Returns 1 if enabled, 0 if not.
 */
SL_EXTERN int SLutf8_enable (int);
SL_EXTERN int SLutf8_is_utf8_mode (void);
SL_EXTERN int SLtt_utf8_enable (int);
SL_EXTERN int SLtt_is_utf8_mode (void);
SL_EXTERN int SLsmg_utf8_enable (int);
SL_EXTERN int SLsmg_is_utf8_mode (void);
SL_EXTERN int SLinterp_utf8_enable (int);
SL_EXTERN int SLinterp_is_utf8_mode (void);
  
SL_EXTERN SLwchar_Type SLwchar_toupper (SLwchar_Type);
SL_EXTERN SLwchar_Type SLwchar_tolower (SLwchar_Type);

SL_EXTERN int SLwchar_wcwidth (SLwchar_Type);
SL_EXTERN int SLwchar_isalnum (SLwchar_Type);
SL_EXTERN int SLwchar_isalpha (SLwchar_Type);
SL_EXTERN int SLwchar_isblank (SLwchar_Type);
SL_EXTERN int SLwchar_iscntrl (SLwchar_Type);
SL_EXTERN int SLwchar_isdigit (SLwchar_Type);
SL_EXTERN int SLwchar_isgraph (SLwchar_Type);
SL_EXTERN int SLwchar_islower (SLwchar_Type);
SL_EXTERN int SLwchar_isprint (SLwchar_Type);
SL_EXTERN int SLwchar_ispunct (SLwchar_Type);
SL_EXTERN int SLwchar_isspace (SLwchar_Type);
SL_EXTERN int SLwchar_isupper (SLwchar_Type);
SL_EXTERN int SLwchar_isxdigit (SLwchar_Type);

SL_EXTERN SLuchar_Type *SLutf8_skip_char (SLuchar_Type *u, SLuchar_Type *umax);
SL_EXTERN SLuchar_Type *SLutf8_bskip_char (SLuchar_Type *umin, SLuchar_Type *u);

/* The SLutf8_strup/lo functions return slstrings -- free with SLang_free_slstring */
SL_EXTERN SLuchar_Type *SLutf8_strup (SLuchar_Type *u, SLuchar_Type *umax);
SL_EXTERN SLuchar_Type *SLutf8_strlo (SLuchar_Type *u, SLuchar_Type *umax);

SL_EXTERN SLuchar_Type *SLutf8_skip_chars (SLuchar_Type *u, SLuchar_Type *umax,
					unsigned int num, unsigned int *dnum,
					int ignore_combining );
SL_EXTERN SLuchar_Type *SLutf8_bskip_chars (SLuchar_Type *umin, SLuchar_Type *u,
					  unsigned int num, unsigned int *dnum,
					 int ignore_combining);

SL_EXTERN SLstr_Type *SLutf8_subst_wchar (SLuchar_Type *u, SLuchar_Type *umax,
				       SLwchar_Type wch, unsigned int pos,
				       int ignore_combining);


SL_EXTERN unsigned int SLutf8_strlen (SLuchar_Type *s, int ignore_combining);
SL_EXTERN SLuchar_Type *SLutf8_decode (SLuchar_Type *u, SLuchar_Type *umax,
				     SLwchar_Type *w, unsigned int *nconsumedp);
SL_EXTERN SLuchar_Type *SLutf8_encode (SLwchar_Type w, SLuchar_Type *u, unsigned int ulen);

SL_EXTERN int SLutf8_compare (SLuchar_Type *a, SLuchar_Type *amax, 
                           SLuchar_Type *b, SLuchar_Type *bmax,
                           unsigned int nchars, int case_sensitive);

/* In these functions, buf is assumed to contain at least SLUTF8_MAX_MBLEN+1 
 * bytes 
 */
SL_EXTERN SLuchar_Type *SLutf8_extract_utf8_char (SLuchar_Type *u, SLuchar_Type *umax, SLuchar_Type *buf);
SL_EXTERN SLuchar_Type *SLutf8_encode_null_terminate (SLwchar_Type w, SLuchar_Type *buf);


typedef struct SLwchar_Lut_Type SLwchar_Lut_Type;
SL_EXTERN SLwchar_Lut_Type *SLwchar_create_lut (unsigned int num_entries);
SL_EXTERN int SLwchar_add_range_to_lut (SLwchar_Lut_Type *r, SLwchar_Type a, SLwchar_Type b);
SL_EXTERN SLuchar_Type *SLwchar_skip_range (SLwchar_Lut_Type *r, SLuchar_Type *p,
					 SLuchar_Type *pmax, int ignore_combining,
					 int invert);
SL_EXTERN SLwchar_Lut_Type *SLwchar_strtolut (SLuchar_Type *u, 
					   int allow_range, int allow_charclass);
SL_EXTERN void SLwchar_free_lut (SLwchar_Lut_Type *r);
SL_EXTERN SLuchar_Type *SLwchar_bskip_range (SLwchar_Lut_Type *r, SLuchar_Type *pmin,
					  SLuchar_Type *p,
					  int ignore_combining,
					  int invert);
SL_EXTERN int SLwchar_in_lut (SLwchar_Lut_Type *r, SLwchar_Type wch);


typedef struct SLwchar_Map_Type SLwchar_Map_Type;
SL_EXTERN void SLwchar_free_char_map (SLwchar_Map_Type *map);
SL_EXTERN SLwchar_Map_Type *SLwchar_allocate_char_map (SLuchar_Type *from, SLuchar_Type *to);
SL_EXTERN int SLwchar_apply_char_map (SLwchar_Map_Type *map, SLwchar_Type *input, SLwchar_Type *output, unsigned int num);

/* This function returns a malloced string */
SLuchar_Type *SLuchar_apply_char_map (SLwchar_Map_Type *map, SLuchar_Type *str);


/*}}}*/

/*{{{ Interpreter Typedefs */

typedef unsigned int SLtype;
/* typedef unsigned short SLtype; */

typedef struct _pSLang_Name_Type
{
   char *name;
   struct _pSLang_Name_Type *next;
   unsigned char name_type;
   /* These values here map directly to byte codes.  See _slang.h.
    */
#define SLANG_LVARIABLE		0x01
#define SLANG_GVARIABLE 	0x02
#define SLANG_IVARIABLE 	0x03           /* intrinsic variables */
   /* Note!!! For Macro MAKE_VARIABLE below to work, SLANG_IVARIABLE Must
    be 1 less than SLANG_RVARIABLE!!! */
#define SLANG_RVARIABLE		0x04	       /* read only variable */
#define SLANG_INTRINSIC 	0x05
#define SLANG_FUNCTION  	0x06
#define SLANG_MATH_UNARY  	0x07
#define SLANG_APP_UNARY  	0x08
#define SLANG_ARITH_UNARY	0x09   /* private */
#define SLANG_ARITH_BINARY	0x0A
#define SLANG_ICONSTANT		0x0B
#define SLANG_DCONSTANT		0x0C
#define SLANG_FCONSTANT		0x0D
#define SLANG_LLCONSTANT	0x0E
#define SLANG_PVARIABLE		0x0F   /* private */
#define SLANG_PFUNCTION		0x10   /* private */
#define SLANG_HCONSTANT		0x11
#define SLANG_LCONSTANT		0x12
   /* Rest of fields depend on name type */
}
SLang_Name_Type;

typedef struct
{
   char *name;
   struct _pSLang_Name_Type *next;      /* this is for the hash table */
   char name_type;

   FVOID_STAR i_fun;		       /* address of object */

   /* Do not change this without modifying slang.c:execute_intrinsic_fun */
#define SLANG_MAX_INTRIN_ARGS	7
   SLtype arg_types [SLANG_MAX_INTRIN_ARGS];
   unsigned char num_args;
   SLtype return_type;
}
SLang_Intrin_Fun_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   VOID_STAR addr;
   SLtype type;
}
SLang_Intrin_Var_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   int unary_op;
}
SLang_App_Unary_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   int unary_op;
}
SLang_Math_Unary_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   SLtype data_type;
   short value;
}
SLang_HConstant_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   SLtype data_type;
   int value;
}
SLang_IConstant_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   SLtype data_type;
   long value;
}
SLang_LConstant_Type;

#ifdef HAVE_LONG_LONG
typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;

   long long ll;
}
SLang_LLConstant_Type;
#endif

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;
   double d;
}
SLang_DConstant_Type;

typedef struct
{
   char *name;
   SLang_Name_Type *next;
   char name_type;
   float f;
}
SLang_FConstant_Type;

typedef struct
{
   char *field_name;		       /* gets replaced by slstring at run-time */
   unsigned int offset;
   SLtype type;
   unsigned char read_only;
}
SLang_IStruct_Field_Type;

typedef SLCONST struct _pSLang_CStruct_Field_Type   /* a g++ bug?? yuk*/
{
   char *field_name;
   unsigned int offset;
   SLtype type;
   unsigned char read_only;
}
SLang_CStruct_Field_Type;

SL_EXTERN int SLadd_intrin_fun_table (SLang_Intrin_Fun_Type *, char *);
SL_EXTERN int SLadd_intrin_var_table (SLang_Intrin_Var_Type *, char *);
SL_EXTERN int SLadd_app_unary_table (SLang_App_Unary_Type *, char *);
SL_EXTERN int SLadd_math_unary_table (SLang_Math_Unary_Type *, char *);
SL_EXTERN int SLadd_iconstant_table (SLang_IConstant_Type *, char *);
SL_EXTERN int SLadd_dconstant_table (SLang_DConstant_Type *, char *);
SL_EXTERN int SLadd_fconstant_table (SLang_FConstant_Type *, char *);
#if HAVE_LONG_LONG
SL_EXTERN int SLadd_llconstant_table (SLang_LLConstant_Type *, char *);
#endif
SL_EXTERN int SLadd_istruct_table (SLang_IStruct_Field_Type *, VOID_STAR, char *);


typedef struct _pSLang_NameSpace_Type SLang_NameSpace_Type;

SL_EXTERN int SLns_add_intrin_fun_table (SLang_NameSpace_Type *, SLang_Intrin_Fun_Type *, char *);
SL_EXTERN int SLns_add_intrin_var_table (SLang_NameSpace_Type *, SLang_Intrin_Var_Type *, char *);
SL_EXTERN int SLns_add_app_unary_table (SLang_NameSpace_Type *, SLang_App_Unary_Type *, char *);
SL_EXTERN int SLns_add_math_unary_table (SLang_NameSpace_Type *, SLang_Math_Unary_Type *, char *);
SL_EXTERN int SLns_add_hconstant_table (SLang_NameSpace_Type *, SLang_HConstant_Type *, char *);
SL_EXTERN int SLns_add_iconstant_table (SLang_NameSpace_Type *, SLang_IConstant_Type *, char *);
SL_EXTERN int SLns_add_lconstant_table (SLang_NameSpace_Type *, SLang_LConstant_Type *, char *);
SL_EXTERN int SLns_add_fconstant_table (SLang_NameSpace_Type *, SLang_FConstant_Type *, char *);
SL_EXTERN int SLns_add_dconstant_table (SLang_NameSpace_Type *, SLang_DConstant_Type *, char *);
#ifdef HAVE_LONG_LONG
SL_EXTERN int SLns_add_llconstant_table (SLang_NameSpace_Type *, SLang_LLConstant_Type *, char *);
#endif
SL_EXTERN int SLns_add_istruct_table (SLang_NameSpace_Type *, SLang_IStruct_Field_Type *, VOID_STAR, char *);

SL_EXTERN int SLns_add_hconstant (SLang_NameSpace_Type *, char *, SLtype, short);
SL_EXTERN int SLns_add_iconstant (SLang_NameSpace_Type *, char *, SLtype, int);
SL_EXTERN int SLns_add_lconstant (SLang_NameSpace_Type *, char *, SLtype, long);
SL_EXTERN int SLns_add_fconstant (SLang_NameSpace_Type *, char *, float);
SL_EXTERN int SLns_add_dconstant (SLang_NameSpace_Type *, char *, double);
#ifdef HAVE_LONG_LONG
SL_EXTERN int SLns_add_llconstant (SLang_NameSpace_Type *, char *, long long);
#endif
SL_EXTERN SLang_NameSpace_Type *SLns_create_namespace (char *);
SL_EXTERN void SLns_delete_namespace (SLang_NameSpace_Type *);

SL_EXTERN int SLns_load_file (char *, char *);
SL_EXTERN int SLns_load_string (char *, char *);
SL_EXTERN int (*SLns_Load_File_Hook) (char *, char *);
SL_EXTERN int SLang_load_file_verbose (int);    
/* if non-zero, display file loading messages */

typedef struct SLang_Load_Type
{
   int type;

   VOID_STAR client_data;
   /* Pointer to data that client needs for loading */

   int auto_declare_globals;
   /* if non-zero, undefined global variables are declared as static */

   char *(*read)(struct SLang_Load_Type *);
   /* function to call to read next line from obj. */

   unsigned int line_num;
   /* Number of lines read, used for error reporting */

   int parse_level;
   /* 0 if at top level of parsing */

   char *name;
   /* Name of this object, e.g., filename.  This name should be unique because
    * it alone determines the name space for static objects associated with
    * the compilable unit.
    */

   char *namespace_name;
   unsigned long reserved[3];
   /* For future expansion */
} SLang_Load_Type;

SL_EXTERN SLang_Load_Type *SLallocate_load_type (char *);
SL_EXTERN void SLdeallocate_load_type (SLang_Load_Type *);
SL_EXTERN SLang_Load_Type *SLns_allocate_load_type (char *, char *);
  
/* Returns SLang_Error upon failure */
SL_EXTERN int SLang_load_object (SLang_Load_Type *);
SL_EXTERN int (*SLang_Load_File_Hook)(char *);
SL_EXTERN int (*SLang_Auto_Declare_Var_Hook) (char *);

SL_EXTERN int SLang_generate_debug_info (int);


#if defined(ultrix) && !defined(__GNUC__)
# ifndef NO_PROTOTYPES
#  define NO_PROTOTYPES
# endif
#endif

#ifndef NO_PROTOTYPES
# define _PROTO(x) x
#else
# define _PROTO(x) ()
#endif

typedef struct _pSLang_Struct_Type SLang_Struct_Type;
SL_EXTERN void SLang_free_struct (SLang_Struct_Type *);
SL_EXTERN int SLang_push_struct (SLang_Struct_Type *);
SL_EXTERN int SLang_pop_struct (SLang_Struct_Type **);

typedef struct _pSLang_Foreach_Context_Type SLang_Foreach_Context_Type;

typedef struct _pSLang_Class_Type SLang_Class_Type;

/* These are the low-level functions for building push/pop methods.  They
 * know nothing about memory management.  For SLANG_CLASS_TYPE_MMT, use the
 * MMT push/pop functions instead.
 */
SL_EXTERN int SLclass_push_double_obj (SLtype, double);
SL_EXTERN int SLclass_push_float_obj (SLtype, float);
SL_EXTERN int SLclass_push_long_obj (SLtype, long);
SL_EXTERN int SLclass_push_int_obj (SLtype, int);
SL_EXTERN int SLclass_push_short_obj (SLtype, short);
SL_EXTERN int SLclass_push_char_obj (SLtype, char);
SL_EXTERN int SLclass_push_ptr_obj (SLtype, VOID_STAR);
SL_EXTERN int SLclass_pop_double_obj (SLtype, double *);
SL_EXTERN int SLclass_pop_float_obj (SLtype, float *);
SL_EXTERN int SLclass_pop_long_obj (SLtype, long *);
SL_EXTERN int SLclass_pop_int_obj (SLtype, int *);
SL_EXTERN int SLclass_pop_short_obj (SLtype, short *);
SL_EXTERN int SLclass_pop_char_obj (SLtype, char *);
SL_EXTERN int SLclass_pop_ptr_obj (SLtype, VOID_STAR *);

#ifdef HAVE_LONG_LONG
SL_EXTERN int SLang_pop_long_long (long long *);
SL_EXTERN int SLang_push_long_long (long long);
SL_EXTERN int SLang_pop_ulong_long (unsigned long long *);
SL_EXTERN int SLang_push_ulong_long (unsigned long long);
SL_EXTERN int SLclass_pop_llong_obj (SLtype, long long *);
SL_EXTERN int SLclass_push_llong_obj (SLtype, long long);
#endif

SL_EXTERN SLang_Class_Type *SLclass_allocate_class (char *);
SL_EXTERN int SLclass_get_class_id (SLang_Class_Type *cl);
SL_EXTERN int SLclass_create_synonym (char *, SLtype);
SL_EXTERN int SLclass_is_class_defined (SLtype);
SL_EXTERN int SLclass_dup_object (SLtype type, VOID_STAR from, VOID_STAR to);

typedef int SLclass_Type;
#define SLANG_CLASS_TYPE_MMT		0
#define SLANG_CLASS_TYPE_SCALAR		1
#define SLANG_CLASS_TYPE_VECTOR		2
#define SLANG_CLASS_TYPE_PTR		3
SL_EXTERN int SLclass_register_class (SLang_Class_Type *, SLtype, unsigned int, SLclass_Type);

SL_EXTERN int SLclass_set_string_function (SLang_Class_Type *, char *(*)(SLtype, VOID_STAR));
SL_EXTERN int SLclass_set_destroy_function (SLang_Class_Type *, void (*)(SLtype, VOID_STAR));
SL_EXTERN int SLclass_set_push_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR));
SL_EXTERN int SLclass_set_apush_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR));
SL_EXTERN int SLclass_set_pop_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR));

SL_EXTERN int SLclass_set_aget_function (SLang_Class_Type *, int (*)(SLtype, unsigned int));
SL_EXTERN int SLclass_set_aput_function (SLang_Class_Type *, int (*)(SLtype, unsigned int));
SL_EXTERN int SLclass_set_anew_function (SLang_Class_Type *, int (*)(SLtype, unsigned int));

SL_EXTERN int SLclass_set_sget_function (SLang_Class_Type *, int (*)(SLtype, char *));
SL_EXTERN int SLclass_set_sput_function (SLang_Class_Type *, int (*)(SLtype, char *));

SL_EXTERN int SLclass_set_acopy_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR, VOID_STAR));
SL_EXTERN int SLclass_set_deref_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR));
SL_EXTERN int SLclass_set_eqs_function (SLang_Class_Type *, int (*)(SLtype, VOID_STAR, SLtype, VOID_STAR));

SL_EXTERN int SLclass_set_length_function (SLang_Class_Type *, int(*)(SLtype, VOID_STAR, unsigned int *));

SL_EXTERN int SLclass_set_is_container (SLang_Class_Type *, int);
SL_EXTERN int SLclass_set_foreach_functions (
  SLang_Class_Type *,
  SLang_Foreach_Context_Type *(*)(SLtype, unsigned int),   /* open method */
  int (*)(SLtype, SLang_Foreach_Context_Type *),   /* foreach method */
  void (*)(SLtype, SLang_Foreach_Context_Type *));/* close method */

   
/* Typecast object on the stack to type p1.  p2 and p3 should be set to 1 */
SL_EXTERN int SLclass_typecast (SLtype, int, int);

#define SLMATH_SIN	1
#define SLMATH_COS	2
#define SLMATH_TAN	3
#define SLMATH_ATAN	4
#define SLMATH_ASIN	5
#define SLMATH_ACOS	6
#define SLMATH_EXP	7
#define SLMATH_LOG	8
#define SLMATH_SQRT	9
#define SLMATH_LOG10	10
#define SLMATH_REAL	11
#define SLMATH_IMAG	12
#define SLMATH_SINH	13
#define SLMATH_COSH	14
#define SLMATH_TANH	15
#define SLMATH_ATANH	16
#define SLMATH_ASINH	17
#define SLMATH_ACOSH	18
#define SLMATH_TODOUBLE	19
#define SLMATH_CONJ	20
#define SLMATH_ISINF	21
#define SLMATH_ISNAN	22
#define SLMATH_FLOOR	23
#define SLMATH_CEIL	24
#define SLMATH_ROUND	25

SL_EXTERN int SLclass_add_unary_op (SLtype,
				 int (*) (int,
					  SLtype, VOID_STAR, unsigned int,
					  VOID_STAR),
				 int (*) (int, SLtype, SLtype *));

SL_EXTERN int
SLclass_add_app_unary_op (SLtype,
			  int (*) (int,
				   SLtype, VOID_STAR, unsigned int,
				   VOID_STAR),
			  int (*) (int, SLtype, SLtype *));

SL_EXTERN int
SLclass_add_binary_op (SLtype, SLtype,
		       int (*) (int,
				SLtype, VOID_STAR, unsigned int,
				SLtype, VOID_STAR, unsigned int,
				VOID_STAR),
		       int (*) (int, SLtype, SLtype, SLtype *));

SL_EXTERN int
SLclass_add_math_op (SLtype,
		     int (*)(int,
			     SLtype, VOID_STAR, unsigned int,
			     VOID_STAR),
		     int (*)(int, SLtype, SLtype *));

SL_EXTERN int
SLclass_add_typecast (SLtype /* from */, SLtype /* to */,
		      int (*)_PROTO((SLtype, VOID_STAR, unsigned int,
				     SLtype, VOID_STAR)),
		      int	       /* allow implicit typecasts */
		      );

SL_EXTERN char *SLclass_get_datatype_name (SLtype);

SL_EXTERN double SLcomplex_abs (double *);
SL_EXTERN double *SLcomplex_times (double *, double *, double *);
SL_EXTERN double *SLcomplex_divide (double *, double *, double *);
SL_EXTERN double *SLcomplex_sin (double *, double *);
SL_EXTERN double *SLcomplex_cos (double *, double *);
SL_EXTERN double *SLcomplex_tan (double *, double *);
SL_EXTERN double *SLcomplex_asin (double *, double *);
SL_EXTERN double *SLcomplex_acos (double *, double *);
SL_EXTERN double *SLcomplex_atan (double *, double *);
SL_EXTERN double *SLcomplex_exp (double *, double *);
SL_EXTERN double *SLcomplex_log (double *, double *);
SL_EXTERN double *SLcomplex_log10 (double *, double *);
SL_EXTERN double *SLcomplex_sqrt (double *, double *);
SL_EXTERN double *SLcomplex_sinh (double *, double *);
SL_EXTERN double *SLcomplex_cosh (double *, double *);
SL_EXTERN double *SLcomplex_tanh (double *, double *);
SL_EXTERN double *SLcomplex_pow (double *, double *, double *);
SL_EXTERN double SLmath_hypot (double x, double y);

/* Not implemented yet */
SL_EXTERN double *SLcomplex_asinh (double *, double *);
SL_EXTERN double *SLcomplex_acosh (double *, double *);
SL_EXTERN double *SLcomplex_atanh (double *, double *);

#ifdef SLANG_SOURCE_
typedef struct _pSLang_MMT_Type SLang_MMT_Type;
#else
typedef int SLang_MMT_Type;
#endif

SL_EXTERN void SLang_free_mmt (SLang_MMT_Type *);
SL_EXTERN VOID_STAR SLang_object_from_mmt (SLang_MMT_Type *);
SL_EXTERN SLang_MMT_Type *SLang_create_mmt (SLtype, VOID_STAR);
SL_EXTERN int SLang_push_mmt (SLang_MMT_Type *);
SL_EXTERN SLang_MMT_Type *SLang_pop_mmt (SLtype);
SL_EXTERN void SLang_inc_mmt (SLang_MMT_Type *);

/* Maximum number of dimensions of an array. */
#define SLARRAY_MAX_DIMS		7
typedef int SLindex_Type;
typedef unsigned int SLuindex_Type;
#define SLANG_ARRAY_INDEX_TYPE SLANG_INT_TYPE
typedef struct _pSLang_Array_Type
{
   SLtype data_type;
   unsigned int sizeof_type;
   VOID_STAR data;
   SLuindex_Type num_elements;
   unsigned int num_dims;
   SLindex_Type dims [SLARRAY_MAX_DIMS];
   VOID_STAR (*index_fun)_PROTO((struct _pSLang_Array_Type *, SLindex_Type *));
   /* This function is designed to allow a type to store an array in
    * any manner it chooses.  This function returns the address of the data
    * value at the specified index location.
    */
   unsigned int flags;
#define SLARR_DATA_VALUE_IS_READ_ONLY		1
#define SLARR_DATA_VALUE_IS_POINTER		2
#define SLARR_DATA_VALUE_IS_RANGE		4
#define SLARR_DATA_VALUE_IS_INTRINSIC		8
   SLang_Class_Type *cl;
   unsigned int num_refs;
   void (*free_fun)_PROTO((struct _pSLang_Array_Type *));
   VOID_STAR client_data;
}
SLang_Array_Type;

SL_EXTERN int SLang_pop_array_of_type (SLang_Array_Type **, SLtype);
SL_EXTERN int SLang_pop_array (SLang_Array_Type **, int);
SL_EXTERN int SLang_push_array (SLang_Array_Type *, int);
SL_EXTERN void SLang_free_array (SLang_Array_Type *);
SL_EXTERN SLang_Array_Type *SLang_create_array (SLtype, int, VOID_STAR, SLindex_Type *, unsigned int);
SL_EXTERN SLang_Array_Type *SLang_duplicate_array (SLang_Array_Type *);
SL_EXTERN int SLang_get_array_element (SLang_Array_Type *, SLindex_Type *, VOID_STAR);
SL_EXTERN int SLang_set_array_element (SLang_Array_Type *, SLindex_Type *, VOID_STAR);

typedef int SLarray_Contract_Fun_Type (VOID_STAR xp, unsigned int increment, unsigned int num, VOID_STAR yp);
typedef struct
{
   SLtype from_type;		       /* if array is this type */
   SLtype typecast_to_type;	       /* typecast it to this */
   SLtype result_type;		       /* to produce this */
   SLarray_Contract_Fun_Type *f;       /* via this function */
}
SLarray_Contract_Type;
SL_EXTERN int SLarray_contract_array (SLCONST SLarray_Contract_Type *);

typedef int SLarray_Map_Fun_Type (SLtype xtype, VOID_STAR xp, 
				  unsigned int increment, unsigned int num,
				  SLtype ytype, VOID_STAR yp, VOID_STAR clientdata);
typedef struct
{
   SLtype from_type;		       /* if array is this type */
   SLtype typecast_to_type;	       /* typecast it to this */
   SLtype result_type;		       /* to produce this */
   SLarray_Map_Fun_Type *f;	       /* via this function */
}
SLarray_Map_Type;

SL_EXTERN int SLarray_map_array_1 (SLCONST SLarray_Map_Type *, 
				int *use_this_dim, 
				VOID_STAR clientdata);
SL_EXTERN int SLarray_map_array (SLCONST SLarray_Map_Type *);


/*}}}*/

/*{{{ Interpreter Function Prototypes */

SL_EXTERN void SLang_verror (int, char *, ...) SLATTRIBUTE_PRINTF(2,3);
SL_EXTERN int SLang_get_error (void);
SL_EXTERN int SLang_set_error (int);
SL_EXTERN char *SLerr_strerror (int errcode);
SL_EXTERN int SLerr_new_exception (int baseclass, char *name, char *descript);
SL_EXTERN int SLerr_exception_eqs (int, int);

SL_EXTERN int SL_Any_Error;
SL_EXTERN int SL_OS_Error;
SL_EXTERN int   SL_Malloc_Error;
SL_EXTERN int   SL_IO_Error;
SL_EXTERN int     SL_Write_Error;
SL_EXTERN int     SL_Read_Error;
SL_EXTERN int     SL_Open_Error;
SL_EXTERN int SL_RunTime_Error;
SL_EXTERN int   SL_InvalidParm_Error;
SL_EXTERN int   SL_TypeMismatch_Error;
SL_EXTERN int   SL_UserBreak_Error;
SL_EXTERN int   SL_Stack_Error;
SL_EXTERN int     SL_StackOverflow_Error;
SL_EXTERN int     SL_StackUnderflow_Error;
SL_EXTERN int   SL_ReadOnly_Error;
SL_EXTERN int   SL_VariableUninitialized_Error;
SL_EXTERN int   SL_NumArgs_Error;
SL_EXTERN int   SL_Index_Error;
SL_EXTERN int SL_Parse_Error;
SL_EXTERN int   SL_Syntax_Error;
SL_EXTERN int   SL_DuplicateDefinition_Error;
SL_EXTERN int   SL_UndefinedName_Error;
SL_EXTERN int SL_Usage_Error;
SL_EXTERN int SL_Application_Error;
SL_EXTERN int SL_Internal_Error;
SL_EXTERN int SL_NotImplemented_Error;
SL_EXTERN int SL_LimitExceeded_Error;
SL_EXTERN int SL_Forbidden_Error;
SL_EXTERN int SL_Math_Error;
SL_EXTERN int   SL_DivideByZero_Error;
SL_EXTERN int   SL_ArithOverflow_Error;
SL_EXTERN int   SL_ArithUnderflow_Error;
SL_EXTERN int   SL_Domain_Error;
SL_EXTERN int SL_Data_Error;
SL_EXTERN int SL_Unicode_Error;
SL_EXTERN int   SL_InvalidUTF8_Error;
SL_EXTERN int SL_Namespace_Error;
SL_EXTERN int SL_Unknown_Error;
SL_EXTERN int SL_Import_Error;

/* Non zero if error occurs.  Must be reset to zero to continue. */

/* Compatibility */
#define USER_BREAK			SL_UserBreak_Error
#define INTRINSIC_ERROR			SL_RunTime_Error
#define SL_OBJ_NOPEN			SL_Open_Error

#define SL_UNKNOWN_ERROR	SL_Unknown_Error
#define SL_APPLICATION_ERROR	SL_Application_Error
#define SL_INTERNAL_ERROR	SL_Internal_Error
#define SL_INTRINSIC_ERROR	SL_RunTime_Error
#define SL_NOT_IMPLEMENTED	SL_NotImplemented_Error
#define SL_BUILTIN_LIMIT_EXCEEDED	SL_LimitExceeded_Error
#define SL_MALLOC_ERROR		SL_Malloc_Error
#define SL_USER_BREAK		SL_UserBreak_Error
#define SL_IO_WRITE_ERROR	SL_Write_Error
#define SL_IO_READ_ERROR	SL_Read_Error
#define SL_IO_OPEN_ERROR	SL_Open_Error
#define SL_SYNTAX_ERROR		SL_Syntax_Error
#define SL_STACK_OVERFLOW	SL_StackOverflow_Error
#define SL_STACK_UNDERFLOW	SL_StackUnderflow_Error
#define SL_TYPE_MISMATCH	SL_TypeMismatch_Error
#define SL_READONLY_ERROR	SL_ReadOnly_Error
#define SL_VARIABLE_UNINITIALIZED	SL_VariableUninitialized_Error
#define SL_DUPLICATE_DEFINITION		SL_DuplicateDefinition_Error
#define SL_INVALID_PARM			SL_InvalidParm_Error
#define SL_UNDEFINED_NAME		SL_UndefinedName_Error
#define SL_NUM_ARGS_ERROR		SL_NumArgs_Error
#define SL_INDEX_ERROR			SL_Index_Error
#define SL_DIVIDE_ERROR			SL_DivideByZero_Error
#define SL_MATH_ERROR			SL_Math_Error
#define SL_ARITH_OVERFLOW_ERROR		SL_ArithOverflow_Error
#define SL_ARITH_UNDERFLOW_ERROR	SL_ArithUnderflow_Error
#define SL_USAGE_ERROR			SL_Usage_Error
#define SL_INVALID_DATA_ERROR		SL_Data_Error
#define SL_UNICODE_ERROR		SL_Unicode_Error
#define SL_INVALID_UTF8			SL_InvalidUTF8_Error

  SL_EXTERN int SLang_Traceback;
  /* If non-zero, dump an S-Lang traceback upon error.  Available as
     _traceback in S-Lang. */

  SL_EXTERN char *SLang_User_Prompt;
  /* Prompt to use when reading from stdin */
  SL_EXTERN int SLang_Version;
  SL_EXTERN char *SLang_Version_String;
SL_EXTERN char *SLang_Doc_Dir;

SL_EXTERN void (*SLang_VMessage_Hook) (char *, va_list);
SL_EXTERN void SLang_vmessage (char *, ...) SLATTRIBUTE_PRINTF(1,2);

  SL_EXTERN void (*SLang_Error_Hook)(char *);
  /* Pointer to application dependent error messaging routine.  By default,
     messages are displayed on stderr. */

  SL_EXTERN void (*SLang_Exit_Error_Hook)(char *, va_list);
SL_EXTERN void SLang_exit_error (char *, ...) SLATTRIBUTE_((format (printf, 1, 2), noreturn));
  SL_EXTERN void (*SLang_Dump_Routine)(char *);
  /* Called if S-Lang traceback is enabled as well as other debugging
     routines (e.g., trace).  By default, these messages go to stderr. */

  SL_EXTERN void (*SLang_Interrupt)(void);
  /* function to call whenever inner interpreter is entered.  This is
     a good place to set SLang_Error to USER_BREAK. */

  SL_EXTERN void (*SLang_User_Clear_Error)(void);
  /* function that gets called when '_clear_error' is called. */

  /* If non null, these call C functions before and after a slang function. */
  SL_EXTERN void (*SLang_Enter_Function)(char *);
SL_EXTERN void (*SLang_Exit_Function)(char *);

SL_EXTERN int SLang_Num_Function_Args;

/* This function should be called when a system call is interrupted.  It
 * runs a set of hooks.  If any of the hooks returns -1, then the system call
 * should not be restarted.
 */
SL_EXTERN int SLang_handle_interrupt (void);
SL_EXTERN int SLang_add_interrupt_hook (int (*)(VOID_STAR), VOID_STAR);
SL_EXTERN void SLang_remove_interrupt_hook (int (*)(VOID_STAR), VOID_STAR);

/* Functions: */

SL_EXTERN int SLang_init_all (void);
/* Initializes interpreter and all modules */

SL_EXTERN int SLang_init_slang (void);
/* This function is mandatory and must be called by all applications that
 * use the interpreter
 */
SL_EXTERN int SLang_init_posix_process (void);   /* process specific intrinsics */
SL_EXTERN int SLang_init_stdio (void);    /* fgets, etc. stdio functions  */
SL_EXTERN int SLang_init_posix_dir (void);
SL_EXTERN int SLang_init_ospath (void);

SL_EXTERN int SLang_init_slmath (void);
/* called if math functions sin, cos, etc... are needed. */

/* These functions are obsolete.  Use init_stdio, posix_process, etc. */
SL_EXTERN int SLang_init_slfile (void);
SL_EXTERN int SLang_init_slunix (void);
  
SL_EXTERN int SLang_init_slassoc (void);
/* Assoc Arrays (Hashes) */

SL_EXTERN int SLang_init_array (void);
/* Additional arrays functions: transpose, etc... */

SL_EXTERN int SLang_init_array_extra (void);
/* Additional arrays functions, if any */

SL_EXTERN int SLang_init_signal (void);
/* signal handling within the interpreter */

/* Dynamic linking facility */
SL_EXTERN int SLang_init_import (void);

   SL_EXTERN int SLang_load_file (char *);
   /* Load a file of S-Lang code for interpreting.  If the parameter is
    * NULL, input comes from stdin. */

   SL_EXTERN void SLang_restart(int);
   /* should be called if an error occurs.  If the passed integer is
    * non-zero, items are popped off the stack; otherwise, the stack is
    * left intact.  Any time the stack is believed to be trashed, this routine
    * should be called with a non-zero argument (e.g., if setjmp/longjmp is
    * called). */

   SL_EXTERN int SLang_byte_compile_file(char *, int);
   /* takes a file of S-Lang code and ``byte-compiles'' it for faster
    * loading.  The new filename is equivalent to the old except that a `c' is
    * appended to the name.  (e.g., init.sl --> init.slc).  The second
    * specified the method; currently, it is not used.
    */

   SL_EXTERN int SLang_autoload(char *, char *);
   /* Automatically load S-Lang function p1 from file p2.  This function
      is also available via S-Lang */

   SL_EXTERN int SLang_load_string(char *);
   /* Like SLang_load_file except input is from a null terminated string. */

SL_EXTERN int SLstack_depth(void);

SL_EXTERN int SLdo_pop(void);
SL_EXTERN int SLdo_pop_n(unsigned int);

SL_EXTERN int SLang_push_char (char);
SL_EXTERN int SLang_push_uchar (unsigned char);
SL_EXTERN int SLang_pop_char (char *);
SL_EXTERN int SLang_pop_uchar (unsigned char *);

#define SLang_push_integer SLang_push_int
#define SLang_push_uinteger SLang_push_uint
#define SLang_pop_integer SLang_pop_int
#define SLang_pop_uinteger SLang_pop_uint
SL_EXTERN int SLang_push_int(int);
SL_EXTERN int SLang_push_uint(unsigned int);
SL_EXTERN int SLang_pop_int(int *);
SL_EXTERN int SLang_pop_uint(unsigned int *);

SL_EXTERN int SLang_pop_short(short *);
SL_EXTERN int SLang_pop_ushort(unsigned short *);
SL_EXTERN int SLang_push_short(short);
SL_EXTERN int SLang_push_ushort(unsigned short);

SL_EXTERN int SLang_pop_long(long *);
SL_EXTERN int SLang_pop_ulong(unsigned long *);
SL_EXTERN int SLang_push_long(long);
SL_EXTERN int SLang_push_ulong(unsigned long);

SL_EXTERN int SLang_pop_float(float *);
SL_EXTERN int SLang_push_float(float);

SL_EXTERN int SLang_pop_double(double *);
SL_EXTERN int SLang_push_double(double);

SL_EXTERN int SLang_push_complex (double, double);
SL_EXTERN int SLang_pop_complex (double *, double *);

SL_EXTERN int SLang_push_datatype (SLtype);
SL_EXTERN int SLang_pop_datatype (SLtype *);

SL_EXTERN int SLang_push_malloced_string(char *);
/* The normal SLang_push_string pushes an slstring.  This one converts
 * a normally malloced string to an slstring, and then frees the 
 * malloced string.  So, do NOT use the malloced string after calling
 * this routine because it will be freed!  The routine returns -1 upon 
 * error, but the string will be freed.
 */

SL_EXTERN int SLang_push_string(char *);
SL_EXTERN int SLpop_string (char **);

SL_EXTERN int SLang_push_null (void);
SL_EXTERN int SLang_pop_null (void);

SL_EXTERN int SLang_push_value (SLtype type, VOID_STAR);
SL_EXTERN int SLang_pop_value (SLtype type, VOID_STAR);
SL_EXTERN void SLang_free_value (SLtype type, VOID_STAR);

typedef struct _pSLang_Object_Type SLang_Any_Type;

SL_EXTERN int SLang_pop_anytype (SLang_Any_Type **);
SL_EXTERN int SLang_push_anytype (SLang_Any_Type *);
SL_EXTERN void SLang_free_anytype (SLang_Any_Type *);

#ifdef SLANG_SOURCE_
typedef struct _pSLang_Ref_Type SLang_Ref_Type;
#else
typedef int SLang_Ref_Type;
#endif

SL_EXTERN int SLang_pop_ref (SLang_Ref_Type **);
SL_EXTERN void SLang_free_ref (SLang_Ref_Type *);
SL_EXTERN int SLang_assign_to_ref (SLang_Ref_Type *, SLtype, VOID_STAR);
SL_EXTERN int SLang_assign_nametype_to_ref (SLang_Ref_Type *, SLang_Name_Type *);
SL_EXTERN SLang_Name_Type *SLang_pop_function (void);
SL_EXTERN int SLang_push_function (SLang_Name_Type *);
SL_EXTERN SLang_Name_Type *SLang_get_fun_from_ref (SLang_Ref_Type *);
SL_EXTERN void SLang_free_function (SLang_Name_Type *f);
SL_EXTERN SLang_Name_Type *SLang_copy_function (SLang_Name_Type *);

/* C structure interface */
SL_EXTERN int SLang_push_cstruct (VOID_STAR, SLang_CStruct_Field_Type *);
SL_EXTERN int SLang_pop_cstruct (VOID_STAR, SLang_CStruct_Field_Type *);
SL_EXTERN void SLang_free_cstruct (VOID_STAR, SLang_CStruct_Field_Type *);
SL_EXTERN int SLang_assign_cstruct_to_ref (SLang_Ref_Type *, VOID_STAR, SLang_CStruct_Field_Type *);

   SL_EXTERN int SLang_is_defined(char *);
   /* Return non-zero is p1 is defined otherwise returns 0. */

   SL_EXTERN int SLang_run_hooks(char *, unsigned int, ...);
   /* calls S-Lang function p1 pushing p2 strings in the variable argument
    * list onto the stack first.
    * Returns -1 upon error, 1 if hooks exists and it ran,
    * or 0 if hook does not exist.  Thus it returns non-zero is hook was called.
    */

/* These functions return 1 if the indicated function exists and the function
 * runs without error.  If the function does not exist, the function returns
 * 0.  Otherwise -1 is returned with SLang_Error set appropriately.
 */
SL_EXTERN int SLexecute_function (SLang_Name_Type *);
SL_EXTERN int SLang_execute_function(char *);


SL_EXTERN int SLang_end_arg_list (void);
SL_EXTERN int SLang_start_arg_list (void);


SL_EXTERN int SLang_add_intrinsic_array (char *,   /* name */
				      SLtype,   /* type */
				      int,   /* readonly */
				      VOID_STAR,   /* data */
				      unsigned int, ...);   /* num dims */

SL_EXTERN int SLextract_list_element (char *, unsigned int, char,
				   char *, unsigned int);

/* If utf8_encode is >1, then byte values > 127 will be utf8-encoded. 
 * If the string is already in utf8 form, and utf8 is desired, then use with
 * utf8_encode set to 0.  A value of -1 implies use the value appropriate for
 * the current state of the interpreter.
 */
SL_EXTERN int SLexpand_escaped_string (char *dest, char *src, char *src_max, 
				    int utf8_encode);

SL_EXTERN SLang_Name_Type *SLang_get_function (char *);
SL_EXTERN void SLang_release_function (SLang_Name_Type *);

SL_EXTERN int SLreverse_stack (int);
SL_EXTERN int SLroll_stack (int);
/* If argument p is positive, the top p objects on the stack are rolled
 * up.  If negative, the stack is rolled down.
 */
SL_EXTERN int SLdup_n (int n);
/* Duplicate top n elements of stack */

SL_EXTERN int SLang_peek_at_stack1 (void);
SL_EXTERN int SLang_peek_at_stack (void);
SL_EXTERN int SLang_peek_at_stack_n (unsigned int n);
SL_EXTERN int SLang_peek_at_stack1_n (unsigned int n);

/* Returns type of next object on stack-- -1 upon stack underflow. */
SL_EXTERN void SLmake_lut (unsigned char *, unsigned char *, unsigned char);

   SL_EXTERN int SLang_guess_type (char *);

SL_EXTERN int SLstruct_create_struct (unsigned int,
				   char **,
				   SLtype *,
				   VOID_STAR *);

/*}}}*/

/*{{{ Misc Functions */

/* This is an interface to atexit */
SL_EXTERN int SLang_add_cleanup_function (void (*)(void));

SL_EXTERN char *SLmake_string (char *);
SL_EXTERN char *SLmake_nstring (char *, unsigned int);
/* Returns a null terminated string made from the first n characters of the
 * string.
 */

/* Binary strings */
/* The binary string is an opaque type.  Use the SLbstring_get_pointer function
 * to get a pointer and length.
 */
typedef struct _pSLang_BString_Type SLang_BString_Type;
SL_EXTERN unsigned char *SLbstring_get_pointer (SLang_BString_Type *, unsigned int *);

SL_EXTERN SLang_BString_Type *SLbstring_dup (SLang_BString_Type *);
SL_EXTERN SLang_BString_Type *SLbstring_create (unsigned char *, unsigned int);

/* The create_malloced function used the first argument which is assumed
 * to be a pointer to a len + 1 malloced string.  The extra byte is for
 * \0 termination.
 */
SL_EXTERN SLang_BString_Type *SLbstring_create_malloced (unsigned char *, unsigned int, int);

/* Create a bstring from an slstring */
SL_EXTERN SLang_BString_Type *SLbstring_create_slstring (char *);

SL_EXTERN void SLbstring_free (SLang_BString_Type *);
SL_EXTERN int SLang_pop_bstring (SLang_BString_Type **);
SL_EXTERN int SLang_push_bstring (SLang_BString_Type *);

/* GNU Midnight Commander uses replacements from glib */
#ifndef MIDNIGHT_COMMANDER_CODE

SL_EXTERN char *SLmalloc (unsigned int);

#endif                                /* !MIDNIGHT_COMMANDER_CODE */

SL_EXTERN char *SLcalloc (unsigned int, unsigned int);

#ifndef MIDNIGHT_COMMANDER_CODE

SL_EXTERN void SLfree(char *);	       /* This function handles NULL */
SL_EXTERN char *SLrealloc (char *, unsigned int);

#endif                                /* !MIDNIGHT_COMMANDER_CODE */


SL_EXTERN char *SLcurrent_time_string (void);

SL_EXTERN int SLatoi(unsigned char *);
SL_EXTERN long SLatol (unsigned char *);
SL_EXTERN unsigned long SLatoul (unsigned char *);

#if HAVE_LONG_LONG
SL_EXTERN long long SLatoll (unsigned char *s);
SL_EXTERN unsigned long long SLatoull (unsigned char *s);
#endif
SL_EXTERN int SLang_pop_fileptr (SLang_MMT_Type **, FILE **);
SL_EXTERN char *SLang_get_name_from_fileptr (SLang_MMT_Type *);

SL_EXTERN int SLang_get_fileptr (SLang_MMT_Type *, FILE **);
/* This function may be used to obtain the FILE* object associated with an MMT.
 * It returns 0 if no-errors were encountered, and -1 otherwise.  
 * If FILE* object has been closed, this function will return 0 and set the FILE* 
 * parameter to NULL.
 */

typedef struct _pSLFile_FD_Type SLFile_FD_Type;
SL_EXTERN SLFile_FD_Type *SLfile_create_fd (char *, int);
SL_EXTERN void SLfile_free_fd (SLFile_FD_Type *);
SL_EXTERN int SLfile_push_fd (SLFile_FD_Type *);
SL_EXTERN int SLfile_pop_fd (SLFile_FD_Type **);
SL_EXTERN int SLfile_get_fd (SLFile_FD_Type *, int *);
SL_EXTERN SLFile_FD_Type *SLfile_dup_fd (SLFile_FD_Type *f0);
SL_EXTERN int SLang_init_posix_io (void);

typedef double (*SLang_To_Double_Fun_Type)(VOID_STAR);
SL_EXTERN SLang_To_Double_Fun_Type SLarith_get_to_double_fun (SLtype, unsigned int *);

SL_EXTERN int SLang_set_argc_argv (int, char **);

/*}}}*/

/*{{{ SLang getkey interface Functions */

#ifdef REAL_UNIX_SYSTEM
SL_EXTERN int SLang_TT_Baud_Rate;
SL_EXTERN int SLang_TT_Read_FD;
#else
# if defined(__WIN32__)
/* I do not want to include windows.h just to get the typedef for HANDLE.
 * Make this conditional upon the inclusion of windows.h.
 */
#  ifdef WINVER
SL_EXTERN HANDLE SLw32_Hstdin;
#  endif
# endif
#endif

SL_EXTERN int SLang_init_tty (int, int, int);
/* Initializes the tty for single character input.  If the first parameter *p1
 * is in the range 0-255, it will be used for the abort character;
 * otherwise, (unix only) if it is -1, the abort character will be the one
 * used by the terminal.  If the second parameter p2 is non-zero, flow
 * control is enabled.  If the last parmeter p3 is zero, output processing
 * is NOT turned on.  A value of zero is required for the screen management
 * routines. Returns 0 upon success. In addition, if SLang_TT_Baud_Rate ==
 * 0 when this function is called, SLang will attempt to determine the
 * terminals baud rate.  As far as the SLang library is concerned, if
 * SLang_TT_Baud_Rate is less than or equal to zero, the baud rate is
 * effectively infinite.
 */

SL_EXTERN void SLang_reset_tty (void);
/* Resets tty to what it was prior to a call to SLang_init_tty */
#ifdef REAL_UNIX_SYSTEM
SL_EXTERN void SLtty_set_suspend_state (int);
   /* If non-zero argument, terminal driver will be told to react to the
    * suspend character.  If 0, it will not.
    */
SL_EXTERN int (*SLang_getkey_intr_hook) (void);
#endif

#define SLANG_GETKEY_ERROR 0xFFFF
SL_EXTERN unsigned int SLang_getkey (void);
/* reads a single key from the tty.  If the read fails,  0xFFFF is returned. */

#ifdef IBMPC_SYSTEM
SL_EXTERN int SLgetkey_map_to_ansi (int);
#endif

SL_EXTERN int SLang_ungetkey_string (unsigned char *, unsigned int);
SL_EXTERN int SLang_buffer_keystring (unsigned char *, unsigned int);
SL_EXTERN int SLang_ungetkey (unsigned char);
SL_EXTERN void SLang_flush_input (void);
SL_EXTERN int SLang_input_pending (int);
SL_EXTERN int SLang_Abort_Char;
/* The value of the character (0-255) used to trigger SIGINT */
SL_EXTERN int SLang_Ignore_User_Abort;
/* If non-zero, pressing the abort character will not result in USER_BREAK
 * SLang_Error. */

SL_EXTERN int SLang_set_abort_signal (void (*)(int));
/* If SIGINT is generated, the function p1 will be called.  If p1 is NULL
 * the SLang_default signal handler is called.  This sets SLang_Error to
 * USER_BREAK.  I suspect most users will simply want to pass NULL.
 */
SL_EXTERN unsigned int SLang_Input_Buffer_Len;

SL_EXTERN volatile int SLKeyBoard_Quit;

#ifdef VMS
/* If this function returns -1, ^Y will be added to input buffer. */
SL_EXTERN int (*SLtty_VMS_Ctrl_Y_Hook) (void);
#endif
/*}}}*/

/*{{{ SLang Keymap routines */

typedef struct SLKeymap_Function_Type
{
   char *name;
   int (*f)(void);
}
SLKeymap_Function_Type;

#define SLANG_MAX_KEYMAP_KEY_SEQ	14
typedef struct SLang_Key_Type
{
   struct SLang_Key_Type *next;
   union
     {
	char *s;
	FVOID_STAR f;
	unsigned int keysym;
	SLang_Name_Type *slang_fun;
     }
     f;
   unsigned char type;	       /* type of function */
#define SLKEY_F_INTERPRET	0x01
#define SLKEY_F_INTRINSIC	0x02
#define SLKEY_F_KEYSYM		0x03
#define SLKEY_F_SLANG		0x04
   unsigned char str[SLANG_MAX_KEYMAP_KEY_SEQ + 1];/* key sequence */
}
SLang_Key_Type;

int SLkm_set_free_method (int, void (*)(int, VOID_STAR));

typedef struct _pSLkeymap_Type
{
   char *name;			       /* hashed string */
   SLang_Key_Type *keymap;
   SLKeymap_Function_Type *functions;  /* intrinsic functions */
   struct _pSLkeymap_Type *next;
} SLkeymap_Type;

SL_EXTERN SLkeymap_Type *SLKeyMap_List_Root;   /* linked list of keymaps */

/* backward compat */
typedef SLkeymap_Type SLKeyMap_List_Type;


SL_EXTERN char *SLang_process_keystring(char *);

SL_EXTERN int SLkm_define_key (char *, FVOID_STAR, SLkeymap_Type *);

SL_EXTERN int SLang_define_key(char *, char *, SLkeymap_Type *);
/* Like define_key1 except that p2 is a string that is to be associated with
 * a function in the functions field of p3.
 */

SL_EXTERN int SLkm_define_keysym (char *, unsigned int, SLkeymap_Type *);
SL_EXTERN int SLkm_define_slkey (char *keysequence, SLang_Name_Type *func, SLkeymap_Type *);
SL_EXTERN void SLang_undefine_key(char *, SLkeymap_Type *);

SL_EXTERN SLkeymap_Type *SLang_create_keymap(char *, SLkeymap_Type *);
/* create and returns a pointer to a new keymap named p1 created by copying
 * keymap p2.  If p2 is NULL, it is up to the calling routine to initialize
 * the keymap.
 */

SL_EXTERN char *SLang_make_keystring(unsigned char *);

SL_EXTERN SLang_Key_Type *SLang_do_key(SLkeymap_Type *, int (*)(void));
/* read a key using keymap p1 with getkey function p2 */

SL_EXTERN FVOID_STAR SLang_find_key_function(char *, SLkeymap_Type *);

SL_EXTERN SLkeymap_Type *SLang_find_keymap(char *);

SL_EXTERN int SLang_Last_Key_Char;
SL_EXTERN int SLang_Key_TimeOut_Flag;

/*}}}*/

/*{{{ SLang Readline Interface */

typedef struct _pSLrline_Type SLrline_Type;
SL_EXTERN SLrline_Type *SLrline_open (unsigned int width, unsigned int flags);
#define SL_RLINE_NO_ECHO	1
#define SL_RLINE_USE_ANSI	2
#define SL_RLINE_BLINK_MATCH	4
#define SL_RLINE_UTF8_MODE	8
SL_EXTERN void SLrline_close (SLrline_Type *);

/* This returns a malloced string */
SL_EXTERN char *SLrline_read_line (SLrline_Type *, char *prompt, unsigned int *lenp);

SL_EXTERN int SLrline_bol (SLrline_Type *);
SL_EXTERN int SLrline_eol (SLrline_Type *);
SL_EXTERN int SLrline_del (SLrline_Type *, unsigned int len);
SL_EXTERN int SLrline_ins (SLrline_Type *, char *s, unsigned int len);

SL_EXTERN int SLrline_set_echo (SLrline_Type *, int);
SL_EXTERN int SLrline_set_tab (SLrline_Type *, unsigned int tabwidth);
SL_EXTERN int SLrline_set_point (SLrline_Type *, unsigned int);
SL_EXTERN int SLrline_set_line (SLrline_Type *, char *);
SL_EXTERN int SLrline_set_hscroll (SLrline_Type *, unsigned int);
SL_EXTERN int SLrline_set_display_width (SLrline_Type *, unsigned int);

SL_EXTERN int SLrline_get_echo (SLrline_Type *, int *);
SL_EXTERN int SLrline_get_tab (SLrline_Type *, unsigned int *);
SL_EXTERN int SLrline_get_point (SLrline_Type *, unsigned int *);
SL_EXTERN char *SLrline_get_line (SLrline_Type *);
SL_EXTERN int SLrline_get_hscroll (SLrline_Type *, unsigned int *);
SL_EXTERN int SLrline_get_display_width (SLrline_Type *, unsigned int *);

SL_EXTERN int SLrline_set_update_hook (SLrline_Type *,
				    void (*)(SLrline_Type *rli,
					     char *prompt,
					     char *buf, unsigned int len,
					     unsigned int point, VOID_STAR client_data),
				    VOID_STAR client_data);

SL_EXTERN SLkeymap_Type *SLrline_get_keymap (SLrline_Type *);

SL_EXTERN void SLrline_redraw (SLrline_Type *);
SL_EXTERN int SLrline_save_line (SLrline_Type *);
SL_EXTERN int SLrline_add_to_history (SLrline_Type *, char *);

/* Compatibility */
typedef SLrline_Type SLang_RLine_Info_Type;

/*}}}*/

/*{{{ Low Level Screen Output Interface */

SL_EXTERN unsigned long SLtt_Num_Chars_Output;
SL_EXTERN int SLtt_Baud_Rate;

typedef unsigned long SLtt_Char_Type;

#define SLTT_BOLD_MASK	0x01000000UL
#define SLTT_BLINK_MASK	0x02000000UL
#define SLTT_ULINE_MASK	0x04000000UL
#define SLTT_REV_MASK	0x08000000UL
#define SLTT_ALTC_MASK  0x10000000UL

SL_EXTERN int SLtt_Screen_Rows;
SL_EXTERN int SLtt_Screen_Cols;
SL_EXTERN int SLtt_Term_Cannot_Insert;
SL_EXTERN int SLtt_Term_Cannot_Scroll;
SL_EXTERN int SLtt_Use_Ansi_Colors;
SL_EXTERN int SLtt_Ignore_Beep;
#if defined(REAL_UNIX_SYSTEM)
SL_EXTERN int SLtt_Force_Keypad_Init;
SL_EXTERN int SLang_TT_Write_FD;
#endif

#ifndef IBMPC_SYSTEM
SL_EXTERN char *SLtt_Graphics_Char_Pairs;
#endif

#ifndef __GO32__
#if defined(VMS) || defined(REAL_UNIX_SYSTEM)
SL_EXTERN int SLtt_Blink_Mode;
SL_EXTERN int SLtt_Use_Blink_For_ACS;
SL_EXTERN int SLtt_Newline_Ok;
SL_EXTERN int SLtt_Has_Alt_Charset;
SL_EXTERN int SLtt_Has_Status_Line;       /* if 0, NO.  If > 0, YES, IF -1, ?? */
# ifndef VMS
SL_EXTERN int SLtt_Try_Termcap;
# endif
#endif
#endif

#if defined(IBMPC_SYSTEM)
SL_EXTERN int SLtt_Msdos_Cheap_Video;
#endif

typedef unsigned short SLsmg_Color_Type;
#define SLSMG_MAX_COLORS	0x7FFE /* keep one for BCE */
#define SLSMG_COLOR_MASK	0x7FFF
#define SLSMG_ACS_MASK		0x8000

#define SLSMG_MAX_CHARS_PER_CELL 5
typedef struct
{
   unsigned int nchars;
   SLwchar_Type wchars[SLSMG_MAX_CHARS_PER_CELL];
   SLsmg_Color_Type color;
}
SLsmg_Char_Type;

#define SLSMG_EXTRACT_COLOR(x) ((x).color)
#define SLSMG_EXTRACT_CHAR(x) ((x).wchars[0])

SL_EXTERN int SLtt_flush_output (void);
SL_EXTERN void SLtt_set_scroll_region(int, int);
SL_EXTERN void SLtt_reset_scroll_region(void);
SL_EXTERN void SLtt_reverse_video (int);
SL_EXTERN void SLtt_bold_video (void);
SL_EXTERN void SLtt_begin_insert(void);
SL_EXTERN void SLtt_end_insert(void);
SL_EXTERN void SLtt_del_eol(void);
SL_EXTERN void SLtt_goto_rc (int, int);
SL_EXTERN void SLtt_delete_nlines(int);
SL_EXTERN void SLtt_delete_char(void);
SL_EXTERN void SLtt_erase_line(void);
SL_EXTERN void SLtt_normal_video(void);
SL_EXTERN void SLtt_cls(void);
SL_EXTERN void SLtt_beep(void);
SL_EXTERN void SLtt_reverse_index(int);
SL_EXTERN void SLtt_smart_puts(SLsmg_Char_Type *, SLsmg_Char_Type *, int, int);
SL_EXTERN void SLtt_write_string (char *);
SL_EXTERN void SLtt_putchar(char);
SL_EXTERN int SLtt_init_video (void);
SL_EXTERN int SLtt_reset_video (void);
SL_EXTERN void SLtt_get_terminfo(void);
SL_EXTERN void SLtt_get_screen_size (void);
SL_EXTERN int SLtt_set_cursor_visibility (int);

SL_EXTERN int SLtt_set_mouse_mode (int, int);

#if defined(VMS) || defined(REAL_UNIX_SYSTEM)
SL_EXTERN int SLtt_initialize (char *);
SL_EXTERN void SLtt_enable_cursor_keys(void);
SL_EXTERN void SLtt_set_term_vtxxx(int *);
SL_EXTERN void SLtt_wide_width(void);
SL_EXTERN void SLtt_narrow_width(void);
SL_EXTERN void SLtt_set_alt_char_set (int);
SL_EXTERN int SLtt_write_to_status_line (char *, int);
SL_EXTERN void SLtt_disable_status_line (void);
# ifdef REAL_UNIX_SYSTEM
/* These are termcap/terminfo routines that assume SLtt_initialize has
 * been called.
 */
SL_EXTERN char *SLtt_tgetstr (char *);
SL_EXTERN int SLtt_tgetnum (char *);
SL_EXTERN int SLtt_tgetflag (char *);

/* The following are terminfo-only routines -- these prototypes will change
 * in V2.x.
 */
SL_EXTERN char *SLtt_tigetent (char *);
SL_EXTERN char *SLtt_tigetstr (char *, char **);
SL_EXTERN int SLtt_tigetnum (char *, char **);
# endif
#endif

SL_EXTERN SLtt_Char_Type SLtt_get_color_object (int);
SL_EXTERN int SLtt_set_color_object (int, SLtt_Char_Type);
SL_EXTERN int SLtt_set_color (int, char *, char *, char *);
SL_EXTERN int SLtt_set_mono (int, char *, SLtt_Char_Type);
SL_EXTERN int SLtt_add_color_attribute (int, SLtt_Char_Type);
SL_EXTERN int SLtt_set_color_fgbg (int, SLtt_Char_Type, SLtt_Char_Type);

/*}}}*/

/*{{{ SLang Preprocessor Interface */

/* #define SLPreprocess_Type SLprep_Type; */
typedef struct _pSLprep_Type SLprep_Type;

SL_EXTERN SLprep_Type *SLprep_new (void);
SL_EXTERN void SLprep_delete (SLprep_Type *);
SL_EXTERN int SLprep_line_ok (char *, SLprep_Type *);
SL_EXTERN int SLprep_set_flags (SLprep_Type *, unsigned int flags);
#define SLPREP_BLANK_LINES_OK	0x1
#define SLPREP_COMMENT_LINES_OK	0x2

SL_EXTERN int SLprep_set_comment (SLprep_Type *, char *, char *);
SL_EXTERN int SLprep_set_prefix (SLprep_Type *, char *);
SL_EXTERN int SLprep_set_exists_hook (SLprep_Type *, 
				   int (*)(SLprep_Type *, char *));
SL_EXTERN int SLprep_set_eval_hook (SLprep_Type *, 
				 int (*)(SLprep_Type *, char *));

SL_EXTERN int SLdefine_for_ifdef (char *);
   /* Adds a string to the SLang #ifdef preparsing defines. SLang already
      defines MSDOS, UNIX, and VMS on the appropriate system. */

/*}}}*/

/*{{{ SLsmg Screen Management Functions */

SL_EXTERN void SLsmg_fill_region (int, int, unsigned int, unsigned int, 
			       SLwchar_Type);
SL_EXTERN void SLsmg_set_char_set (int);
#ifndef IBMPC_SYSTEM
SL_EXTERN int SLsmg_Scroll_Hash_Border;
#endif
SL_EXTERN int SLsmg_suspend_smg (void);
SL_EXTERN int SLsmg_resume_smg (void);
SL_EXTERN void SLsmg_erase_eol (void);
SL_EXTERN void SLsmg_gotorc (int, int);
SL_EXTERN void SLsmg_erase_eos (void);
SL_EXTERN void SLsmg_reverse_video (void);
SL_EXTERN void SLsmg_set_color (SLsmg_Color_Type);
SL_EXTERN void SLsmg_normal_video (void);
SL_EXTERN void SLsmg_printf (char *, ...) SLATTRIBUTE_PRINTF(1,2);
/* SL_EXTERN void SLsmg_printf (char *, ...) SLATTRIBUTE_PRINTF(1,2); */
SL_EXTERN void SLsmg_vprintf (char *, va_list);
SL_EXTERN void SLsmg_write_string (char *);
SL_EXTERN void SLsmg_write_nstring (char *, unsigned int);
SL_EXTERN void SLsmg_write_chars (SLuchar_Type *u, SLuchar_Type *umax);
SL_EXTERN void SLsmg_write_nchars (char *str, unsigned int len);
SL_EXTERN void SLsmg_write_char (SLwchar_Type ch);
SL_EXTERN void SLsmg_write_wrapped_string (SLuchar_Type *, int, int, unsigned int, unsigned int, int);
SL_EXTERN void SLsmg_cls (void);
SL_EXTERN void SLsmg_refresh (void);
SL_EXTERN void SLsmg_touch_lines (int, unsigned int);
SL_EXTERN void SLsmg_touch_screen (void);
SL_EXTERN int SLsmg_init_smg (void);
SL_EXTERN int SLsmg_reinit_smg (void);
SL_EXTERN void SLsmg_reset_smg (void);
SL_EXTERN int SLsmg_char_at (SLsmg_Char_Type *);
SL_EXTERN void SLsmg_set_screen_start (int *, int *);
SL_EXTERN void SLsmg_draw_hline (unsigned int);
SL_EXTERN void SLsmg_draw_vline (int);
SL_EXTERN void SLsmg_draw_object (int, int, SLwchar_Type);
SL_EXTERN void SLsmg_draw_box (int, int, unsigned int, unsigned int);
SL_EXTERN int SLsmg_get_column(void);
SL_EXTERN int SLsmg_get_row(void);
SL_EXTERN void SLsmg_forward (int);
SL_EXTERN void SLsmg_write_color_chars (SLsmg_Char_Type *, unsigned int);
SL_EXTERN unsigned int SLsmg_read_raw (SLsmg_Char_Type *, unsigned int);
SL_EXTERN unsigned int SLsmg_write_raw (SLsmg_Char_Type *, unsigned int);
SL_EXTERN void SLsmg_set_color_in_region (int, int, int, unsigned int, unsigned int);

SL_EXTERN unsigned int SLsmg_strwidth (SLuchar_Type *u, SLuchar_Type *max);
SL_EXTERN unsigned int SLsmg_strbytes (SLuchar_Type *u, SLuchar_Type *max, unsigned int width);
SL_EXTERN int SLsmg_embedded_escape_mode (int on_or_off);
SL_EXTERN int SLsmg_Display_Eight_Bit;
SL_EXTERN int SLsmg_Tab_Width;

#define SLSMG_NEWLINE_IGNORED	0      /* default */
#define SLSMG_NEWLINE_MOVES	1      /* moves to next line, column 0 */
#define SLSMG_NEWLINE_SCROLLS	2      /* moves but scrolls at bottom of screen */
#define SLSMG_NEWLINE_PRINTABLE	3      /* prints as ^J */
SL_EXTERN int SLsmg_Newline_Behavior;

SL_EXTERN int SLsmg_Backspace_Moves;

#ifdef IBMPC_SYSTEM
# define SLSMG_HLINE_CHAR	0xC4
# define SLSMG_VLINE_CHAR	0xB3
# define SLSMG_ULCORN_CHAR	0xDA
# define SLSMG_URCORN_CHAR	0xBF
# define SLSMG_LLCORN_CHAR	0xC0
# define SLSMG_LRCORN_CHAR	0xD9
# define SLSMG_RTEE_CHAR	0xB4
# define SLSMG_LTEE_CHAR	0xC3
# define SLSMG_UTEE_CHAR	0xC2
# define SLSMG_DTEE_CHAR	0xC1
# define SLSMG_PLUS_CHAR	0xC5
/* There are several to choose from: 0xB0, 0xB1, and 0xB2 */
# define SLSMG_CKBRD_CHAR	0xB0
# define SLSMG_DIAMOND_CHAR	0x04
# define SLSMG_DEGREE_CHAR	0xF8
# define SLSMG_PLMINUS_CHAR	0xF1
# define SLSMG_BULLET_CHAR	0xF9
# define SLSMG_LARROW_CHAR	0x1B
# define SLSMG_RARROW_CHAR	0x1A
# define SLSMG_DARROW_CHAR	0x19
# define SLSMG_UARROW_CHAR	0x18
# define SLSMG_BOARD_CHAR	0xB2
# define SLSMG_BLOCK_CHAR	0xDB
#else
# if defined(AMIGA)
#  define SLSMG_HLINE_CHAR	((unsigned char)'-')
#  define SLSMG_VLINE_CHAR	((unsigned char)'|')
#  define SLSMG_ULCORN_CHAR	((unsigned char)'+')
#  define SLSMG_URCORN_CHAR	((unsigned char)'+')
#  define SLSMG_LLCORN_CHAR	((unsigned char)'+')
#  define SLSMG_LRCORN_CHAR	((unsigned char)'+')
#  define SLSMG_CKBRD_CHAR	((unsigned char)'#')
#  define SLSMG_RTEE_CHAR	((unsigned char)'+')
#  define SLSMG_LTEE_CHAR	((unsigned char)'+')
#  define SLSMG_UTEE_CHAR	((unsigned char)'+')
#  define SLSMG_DTEE_CHAR	((unsigned char)'+')
#  define SLSMG_PLUS_CHAR	((unsigned char)'+')
#  define SLSMG_DIAMOND_CHAR	((unsigned char)'+')
#  define SLSMG_DEGREE_CHAR	((unsigned char)'\\')
#  define SLSMG_PLMINUS_CHAR	((unsigned char)'#')
#  define SLSMG_BULLET_CHAR	((unsigned char)'o')
#  define SLSMG_LARROW_CHAR	((unsigned char)'<')
#  define SLSMG_RARROW_CHAR	((unsigned char)'>')
#  define SLSMG_DARROW_CHAR	((unsigned char)'v')
#  define SLSMG_UARROW_CHAR	((unsigned char)'^')
#  define SLSMG_BOARD_CHAR	((unsigned char)'#')
#  define SLSMG_BLOCK_CHAR	((unsigned char)'#')
# else
#  define SLSMG_HLINE_CHAR	((unsigned char)'q')
#  define SLSMG_VLINE_CHAR	((unsigned char)'x')
#  define SLSMG_ULCORN_CHAR	((unsigned char)'l')
#  define SLSMG_URCORN_CHAR	((unsigned char)'k')
#  define SLSMG_LLCORN_CHAR	((unsigned char)'m')
#  define SLSMG_LRCORN_CHAR	((unsigned char)'j')
#  define SLSMG_CKBRD_CHAR	((unsigned char)'a')
#  define SLSMG_RTEE_CHAR	((unsigned char)'u')
#  define SLSMG_LTEE_CHAR	((unsigned char)'t')
#  define SLSMG_UTEE_CHAR	((unsigned char)'w')
#  define SLSMG_DTEE_CHAR	((unsigned char)'v')
#  define SLSMG_PLUS_CHAR	((unsigned char)'n')
#  define SLSMG_DIAMOND_CHAR	((unsigned char)'`')
#  define SLSMG_DEGREE_CHAR	((unsigned char)'f')
#  define SLSMG_PLMINUS_CHAR	((unsigned char)'g')
#  define SLSMG_BULLET_CHAR	((unsigned char)'~')
#  define SLSMG_LARROW_CHAR	((unsigned char)',')
#  define SLSMG_RARROW_CHAR	((unsigned char)'+')
#  define SLSMG_DARROW_CHAR	((unsigned char)'.')
#  define SLSMG_UARROW_CHAR	((unsigned char)'-')
#  define SLSMG_BOARD_CHAR	((unsigned char)'h')
#  define SLSMG_BLOCK_CHAR	((unsigned char)'0')
# endif				       /* AMIGA */
#endif				       /* IBMPC_SYSTEM */

#ifndef IBMPC_SYSTEM
# define SLSMG_COLOR_BLACK		0x000000
# define SLSMG_COLOR_RED		0x000001
# define SLSMG_COLOR_GREEN		0x000002
# define SLSMG_COLOR_BROWN		0x000003
# define SLSMG_COLOR_BLUE		0x000004
# define SLSMG_COLOR_MAGENTA		0x000005
# define SLSMG_COLOR_CYAN		0x000006
# define SLSMG_COLOR_LGRAY		0x000007
# define SLSMG_COLOR_GRAY		0x000008
# define SLSMG_COLOR_BRIGHT_RED		0x000009
# define SLSMG_COLOR_BRIGHT_GREEN	0x00000A
# define SLSMG_COLOR_BRIGHT_BROWN	0x00000B
# define SLSMG_COLOR_BRIGHT_BLUE	0x00000C
# define SLSMG_COLOR_BRIGHT_CYAN	0x00000D
# define SLSMG_COLOR_BRIGHT_MAGENTA	0x00000E
# define SLSMG_COLOR_BRIGHT_WHITE	0x00000F
#endif

typedef struct
{
   void (*tt_normal_video)(void);
   void (*tt_set_scroll_region)(int, int);
   void (*tt_goto_rc)(int, int);
   void (*tt_reverse_index)(int);
   void (*tt_reset_scroll_region)(void);
   void (*tt_delete_nlines)(int);
   void (*tt_cls) (void);
   void (*tt_del_eol) (void);
   void (*tt_smart_puts) (SLsmg_Char_Type *, SLsmg_Char_Type *, int, int);
   int (*tt_flush_output) (void);
   int (*tt_reset_video) (void);
   int (*tt_init_video) (void);

   int *tt_screen_rows;
   int *tt_screen_cols;

   int *tt_term_cannot_scroll;
#if 0
   int *tt_use_blink_for_acs;
#endif
   int *tt_has_alt_charset;
   char **tt_graphic_char_pairs;
   int *unicode_ok;

   long reserved[4];
}
SLsmg_Term_Type;
SL_EXTERN void SLsmg_set_terminal_info (SLsmg_Term_Type *);

/*}}}*/

/*{{{ SLang Keypad Interface */

#define SL_KEY_ERR		0xFFFF

#define SL_KEY_UP		0x101
#define SL_KEY_DOWN		0x102
#define SL_KEY_LEFT		0x103
#define SL_KEY_RIGHT		0x104
#define SL_KEY_PPAGE		0x105
#define SL_KEY_NPAGE		0x106
#define SL_KEY_HOME		0x107
#define SL_KEY_END		0x108
#define SL_KEY_A1		0x109
#define SL_KEY_A3		0x10A
#define SL_KEY_B2		0x10B
#define SL_KEY_C1		0x10C
#define SL_KEY_C3		0x10D
#define SL_KEY_REDO		0x10E
#define SL_KEY_UNDO		0x10F
#define SL_KEY_BACKSPACE	0x110
#define SL_KEY_ENTER		0x111
#define SL_KEY_IC		0x112
#define SL_KEY_DELETE		0x113

#define SL_KEY_F0		0x200
#define SL_KEY_F(X)		(SL_KEY_F0 + X)

/* I do not intend to use keysymps > 0x1000.  Applications can use those. */
/* Returns 0 upon success or -1 upon error. */
SL_EXTERN int SLkp_define_keysym (char *, unsigned int);

/* This function must be called AFTER SLtt_get_terminfo and not before. */
SL_EXTERN int SLkp_init (void);

/* By default, SLang_getkey is used as the low-level function.  This hook
 * allows you to specify something else.
 */
SL_EXTERN void SLkp_set_getkey_function (int (*)(void));

/* This function uses SLang_getkey and assumes that what ever initialization
 * is required for SLang_getkey has been performed.  If you do not want 
 * SLang_getkey to be used, then specify another function via
 * SLkp_set_getkey_function.
 */
SL_EXTERN int SLkp_getkey (void);

/*}}}*/

/*{{{ SLang Scroll Interface */

typedef struct _pSLscroll_Type
{
   struct _pSLscroll_Type *next;
   struct _pSLscroll_Type *prev;
   unsigned int flags;
}
SLscroll_Type;

typedef struct
{
   unsigned int flags;
   SLscroll_Type *top_window_line;   /* list element at top of window */
   SLscroll_Type *bot_window_line;   /* list element at bottom of window */
   SLscroll_Type *current_line;    /* current list element */
   SLscroll_Type *lines;	       /* first list element */
   unsigned int nrows;		       /* number of rows in window */
   unsigned int hidden_mask;	       /* applied to flags in SLscroll_Type */
   unsigned int line_num;	       /* current line number (visible) */
   unsigned int num_lines;	       /* total number of lines (visible) */
   unsigned int window_row;	       /* row of current_line in window */
   unsigned int border;		       /* number of rows that form scroll border */
   int cannot_scroll;		       /* should window scroll or recenter */
}
SLscroll_Window_Type;

SL_EXTERN int SLscroll_find_top (SLscroll_Window_Type *);
SL_EXTERN int SLscroll_find_line_num (SLscroll_Window_Type *);
SL_EXTERN unsigned int SLscroll_next_n (SLscroll_Window_Type *, unsigned int);
SL_EXTERN unsigned int SLscroll_prev_n (SLscroll_Window_Type *, unsigned int);
SL_EXTERN int SLscroll_pageup (SLscroll_Window_Type *);
SL_EXTERN int SLscroll_pagedown (SLscroll_Window_Type *);

/*}}}*/

/*{{{ Signal Routines */

typedef void SLSig_Fun_Type (int);
SL_EXTERN SLSig_Fun_Type *SLsignal (int, SLSig_Fun_Type *);
SL_EXTERN SLSig_Fun_Type *SLsignal_intr (int, SLSig_Fun_Type *);
SL_EXTERN int SLsig_block_signals (void);
SL_EXTERN int SLsig_unblock_signals (void);
SL_EXTERN int SLsystem (char *);

/* Make a signal off-limits to the interpreter */
SL_EXTERN int SLsig_forbid_signal (int);

SL_EXTERN char *SLerrno_strerror (int);
SL_EXTERN int SLerrno_set_errno (int);

/*}}}*/

/* Functions for dealing with the FPU */
SL_EXTERN void SLfpu_clear_except_bits (void);
SL_EXTERN unsigned int SLfpu_test_except_bits (unsigned int bits);
#define SL_FE_DIVBYZERO		0x01
#define SL_FE_INVALID		0x02
#define SL_FE_OVERFLOW		0x04
#define SL_FE_UNDERFLOW		0x08
#define SL_FE_INEXACT		0x10
#define SL_FE_ALLEXCEPT		0x1F

SL_EXTERN SLtype SLang_get_int_type (int nbits);
/* if nbits is negative it gets the signed int type, else unsigned int type */
SL_EXTERN int SLang_get_int_size (SLtype);
/* Opposite of SLang_get_int_type */

/*{{{ Interpreter Macro Definitions */

/* The definitions here are for objects that may be on the run-time stack.
 * They are actually sub_types of literal and data main_types.  The actual
 * numbers are historical.
 */
#define SLANG_UNDEFINED_TYPE	0x00   /* MUST be 0 */
#define SLANG_VOID_TYPE		0x01   /* also matches ANY type */
#define SLANG_NULL_TYPE		(0x02)
#define SLANG_ANY_TYPE		(0x03)
#define SLANG_DATATYPE_TYPE	(0x04)
/* SLANG_REF_TYPE refers to an object on the stack that is a pointer (reference)
 * to some other object.
 */
#define SLANG_REF_TYPE		(0x05)
#define SLANG_STRING_TYPE	(0x06)
#define SLANG_BSTRING_TYPE	(0x07)
#define SLANG_FILE_PTR_TYPE	(0x08)
#define SLANG_FILE_FD_TYPE	(0x09)
#define SLANG_MD5_TYPE		(0x0A)
#define SLANG_INTP_TYPE		(0x0F)

/* Integer types */
/* The integer and floating point types are arranged in order of arithmetic
 * precedence.
 */
#define SLANG_CHAR_TYPE		(0x10)
#define SLANG_UCHAR_TYPE	(0x11)
#define SLANG_SHORT_TYPE	(0x12)
#define SLANG_USHORT_TYPE	(0x13)
#define SLANG_INT_TYPE 		(0x14)
#define SLANG_UINT_TYPE		(0x15)
#define SLANG_LONG_TYPE		(0x16)
#define SLANG_ULONG_TYPE	(0x17)
#define SLANG_LLONG_TYPE	(0x18)
#define SLANG_ULLONG_TYPE	(0x19)
/* floating point types */
#define SLANG_FLOAT_TYPE	(0x1A)
#define SLANG_DOUBLE_TYPE	(0x1B)
#define SLANG_LDOUBLE_TYPE	(0x1C)

#define SLANG_COMPLEX_TYPE	(0x20)

/* An object of SLANG_INTP_TYPE should never really occur on the stack.  Rather,
 * the integer to which it refers will be there instead.  It is defined here
 * because it is a valid type for MAKE_VARIABLE.
 */

/* Container types */
#define SLANG_ISTRUCT_TYPE 	(0x2A)
#define SLANG_STRUCT_TYPE	(0x2B)
#define SLANG_ASSOC_TYPE	(0x2C)
#define SLANG_ARRAY_TYPE	(0x2D)
#define SLANG_LIST_TYPE		(0x2E)


#define SLANG_MIN_UNUSED_TYPE	(0x30)

/* Compatibility */
#ifdef FLOAT_TYPE
# undef FLOAT_TYPE
#endif
#define VOID_TYPE SLANG_VOID_TYPE
#define INT_TYPE SLANG_INT_TYPE
#define INTP_TYPE SLANG_INTP_TYPE
#define FLOAT_TYPE SLANG_DOUBLE_TYPE
#define ARRAY_TYPE SLANG_ARRAY_TYPE
#define CHAR_TYPE SLANG_CHAR_TYPE
#define STRING_TYPE SLANG_STRING_TYPE

/* I am reserving values greater than or equal to 128 for user applications.
 * The first 127 are reserved for S-Lang.
 */

/* Binary and Unary Subtypes */
/* Since the application can define new types and can overload the binary
 * and unary operators, these definitions must be present in this file.
 * The current implementation assumes both unary and binary are distinct.
 */
#define SLANG_BINARY_OP_MIN	0x01
#define SLANG_PLUS		0x01
#define SLANG_MINUS		0x02
#define SLANG_TIMES		0x03
#define SLANG_DIVIDE		0x04
#define SLANG_EQ		0x05
#define SLANG_NE		0x06
#define SLANG_GT		0x07
#define SLANG_GE		0x08
#define SLANG_LT		0x09
#define SLANG_LE		0x0A
#define SLANG_POW		0x0B
#define SLANG_OR		0x0C
#define SLANG_AND		0x0D
#define SLANG_BAND		0x0E
#define SLANG_BOR		0x0F
#define SLANG_BXOR		0x10
#define SLANG_SHL		0x11
#define SLANG_SHR		0x12
#define SLANG_MOD		0x13
#define SLANG_BINARY_OP_MAX	0x13

/* UNARY subtypes  (may be overloaded) */
#define SLANG_UNARY_OP_MIN	0x20

#define SLANG_PLUSPLUS		0x20
#define SLANG_MINUSMINUS	0x21
#define SLANG_CHS		0x22
#define SLANG_NOT		0x23
#define SLANG_BNOT		0x24
/* These are implemented as unary function calls */
#define SLANG_ABS		0x25
#define SLANG_SIGN		0x26
#define SLANG_SQR		0x27
#define SLANG_MUL2		0x28
#define SLANG_ISPOS		0x29
#define SLANG_ISNEG		0x2A
#define SLANG_ISNONNEG		0x2B

#define SLANG_UNARY_OP_MAX	0x2B

SL_EXTERN char *SLang_Error_Message;

SL_EXTERN int SLadd_intrinsic_variable (char *, VOID_STAR, SLtype, int);
SL_EXTERN int SLadd_intrinsic_function (char *, FVOID_STAR, SLtype, unsigned int,...);

SL_EXTERN int SLns_add_intrinsic_variable (SLang_NameSpace_Type *, char *, VOID_STAR, SLtype, int);
SL_EXTERN int SLns_add_intrinsic_function (SLang_NameSpace_Type *, char *, FVOID_STAR, SLtype, unsigned int,...);

#define MAKE_INTRINSIC_N(n,f,out,in,a1,a2,a3,a4,a5,a6,a7) \
    {(n), NULL, SLANG_INTRINSIC, (FVOID_STAR) (f), \
      {a1,a2,a3,a4,a5,a6,a7}, (in), (out)}

#define MAKE_INTRINSIC_7(n,f,out,a1,a2,a3,a4,a5,a6,a7) \
    MAKE_INTRINSIC_N(n,f,out,7,a1,a2,a3,a4,a5,a6,a7)
#define MAKE_INTRINSIC_6(n,f,out,a1,a2,a3,a4,a5,a6) \
    MAKE_INTRINSIC_N(n,f,out,6,a1,a2,a3,a4,a5,a6,0)
#define MAKE_INTRINSIC_5(n,f,out,a1,a2,a3,a4,a5) \
    MAKE_INTRINSIC_N(n,f,out,5,a1,a2,a3,a4,a5,0,0)
#define MAKE_INTRINSIC_4(n,f,out,a1,a2,a3,a4) \
    MAKE_INTRINSIC_N(n,f,out,4,a1,a2,a3,a4,0,0,0)
#define MAKE_INTRINSIC_3(n,f,out,a1,a2,a3) \
    MAKE_INTRINSIC_N(n,f,out,3,a1,a2,a3,0,0,0,0)
#define MAKE_INTRINSIC_2(n,f,out,a1,a2) \
    MAKE_INTRINSIC_N(n,f,out,2,a1,a2,0,0,0,0,0)
#define MAKE_INTRINSIC_1(n,f,out,a1) \
    MAKE_INTRINSIC_N(n,f,out,1,a1,0,0,0,0,0,0)
#define MAKE_INTRINSIC_0(n,f,out) \
    MAKE_INTRINSIC_N(n,f,out,0,0,0,0,0,0,0,0)

#define MAKE_INTRINSIC_S(n,f,r) \
   MAKE_INTRINSIC_1(n,f,r,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_I(n,f,r) \
   MAKE_INTRINSIC_1(n,f,r,SLANG_INT_TYPE)

#define MAKE_INTRINSIC_SS(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SI(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_IS(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_II(n,f,r) \
   MAKE_INTRINSIC_2(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE)

#define MAKE_INTRINSIC_SSS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SSI(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_SIS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_SII(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_STRING_TYPE,SLANG_INT_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_ISS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_ISI(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_STRING_TYPE,SLANG_INT_TYPE)
#define MAKE_INTRINSIC_IIS(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE,SLANG_STRING_TYPE)
#define MAKE_INTRINSIC_III(n,f,r) \
   MAKE_INTRINSIC_3(n,f,r,SLANG_INT_TYPE,SLANG_INT_TYPE,SLANG_INT_TYPE)

#define MAKE_INTRINSIC(n, f, out, in) \
    MAKE_INTRINSIC_N(n,f,out,in,0,0,0,0,0,0,0)

#define MAKE_VARIABLE(n, v, t, r)     \
    {n, NULL, SLANG_IVARIABLE + (r), (VOID_STAR)(v), (t)}

#define MAKE_APP_UNARY(n,op) \
    {(n), NULL, SLANG_APP_UNARY, (op)}

#define MAKE_ARITH_UNARY(n,op) \
    {(n), NULL, SLANG_ARITH_UNARY, (op)}

#define MAKE_ARITH_BINARY(n,op) \
    {(n), NULL, SLANG_ARITH_BINARY, (op)}

#define MAKE_MATH_UNARY(n,op) \
    {(n), NULL, SLANG_MATH_UNARY, (op)}

#define MAKE_HCONSTANT_T(n,val,T) \
    {(n),NULL, SLANG_HCONSTANT, T, (short)(val)}
#define MAKE_HCONSTANT(n,val) MAKE_HCONSTANT_T(n,val,SLANG_SHORT_TYPE)

#define MAKE_ICONSTANT_T(n,val,T) \
    {(n),NULL, SLANG_ICONSTANT, T, (int)(val)}
#define MAKE_ICONSTANT(n,val) MAKE_ICONSTANT_T(n,val,SLANG_INT_TYPE)

#define MAKE_LCONSTANT_T(n,val,T) \
    {(n),NULL, SLANG_LCONSTANT, T, (int)(val)}
#define MAKE_LCONSTANT(n,val) MAKE_LCONSTANT_T(n,val,SLANG_LONG_TYPE)

#ifdef HAVE_LONG_LONG
# define MAKE_LLCONSTANT_T(n,val,T) \
    {(n),NULL, T, (long long)(val)}
# define MAKE_LLCONSTANT(n,val) MAKE_LLCONSTANT_T(n,val,SLANG_LLONG_TYPE)
#endif

#define MAKE_FCONSTANT(n,val) \
    {(n),NULL, SLANG_FCONSTANT, (val)}

#define MAKE_DCONSTANT(n,val) \
    {(n),NULL, SLANG_DCONSTANT, (val)}

#ifndef offsetof
# define offsetof(T,F) ((unsigned int)((char *)&((T *)0L)->F - (char *)0L))
#endif

#define MAKE_ISTRUCT_FIELD(s,f,n,t,r) {(n), offsetof(s,f), (t), (r)}
#define MAKE_CSTRUCT_FIELD(s,f,n,t,r) {(n), offsetof(s,f), (t), (r)}

#define MAKE_CSTRUCT_INT_FIELD(s,f,n,r) {(n), offsetof(s,f),\
   (sizeof(((s*)0L)->f)==sizeof(int))?(SLANG_INT_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(short))?(SLANG_SHORT_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(char))?(SLANG_CHAR_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(long))?(SLANG_LONG_TYPE): \
   SLANG_LLONG_TYPE, (r)\
}
#define MAKE_CSTRUCT_UINT_FIELD(s,f,n,r) {(n), offsetof(s,f),\
   (sizeof(((s*)0L)->f)==sizeof(int))?(SLANG_UINT_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(short))?(SLANG_USHORT_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(char))?(SLANG_UCHAR_TYPE): \
   (sizeof(((s*)0L)->f)==sizeof(long))?(SLANG_ULONG_TYPE): \
   SLANG_ULLONG_TYPE, (r)\
}

#define SLANG_END_TABLE {NULL}
#define SLANG_END_INTRIN_FUN_TABLE MAKE_INTRINSIC_0(NULL,NULL,0)
#define SLANG_END_FCONST_TABLE MAKE_DCONSTANT(NULL,0)
#define SLANG_END_DCONST_TABLE MAKE_DCONSTANT(NULL,0)
#define SLANG_END_MATH_UNARY_TABLE MAKE_MATH_UNARY(NULL,0)
#define SLANG_END_ARITH_UNARY_TABLE MAKE_ARITH_UNARY(NULL,0)
#define SLANG_END_ARITH_BINARY_TABLE MAKE_ARITH_BINARY(NULL,0)
#define SLANG_END_APP_UNARY_TABLE MAKE_APP_UNARY(NULL,0)
#define SLANG_END_INTRIN_VAR_TABLE MAKE_VARIABLE(NULL,NULL,0,0)
#define SLANG_END_ICONST_TABLE MAKE_ICONSTANT(NULL,0)
#define SLANG_END_LLCONST_TABLE MAKE_LLCONSTANT(NULL,0)
#define SLANG_END_ISTRUCT_TABLE {NULL, 0, 0, 0}
#define SLANG_END_CSTRUCT_TABLE {NULL, 0, 0, 0}
   

/*}}}*/

/*{{{ Upper/Lowercase Functions */

SL_EXTERN void SLang_define_case(int *, int *);
SL_EXTERN void SLang_init_case_tables (void);

SL_EXTERN unsigned char _pSLChg_UCase_Lut[256];
SL_EXTERN unsigned char _pSLChg_LCase_Lut[256];
#define UPPER_CASE(x) (_pSLChg_UCase_Lut[(unsigned char) (x)])
#define LOWER_CASE(x) (_pSLChg_LCase_Lut[(unsigned char) (x)])
#define CHANGE_CASE(x) (((x) == _pSLChg_LCase_Lut[(unsigned char) (x)]) ?\
			_pSLChg_UCase_Lut[(unsigned char) (x)] : _pSLChg_LCase_Lut[(unsigned char) (x)])

/*}}}*/

/*{{{ Regular Expression Interface */
typedef struct _pSLRegexp_Type SLRegexp_Type;
SL_EXTERN SLRegexp_Type *SLregexp_compile (char *pattern, unsigned int flags);
#define SLREGEXP_CASELESS	0x01
#define SLREGEXP_UTF8		0x10

SL_EXTERN void SLregexp_free (SLRegexp_Type *);
SL_EXTERN char *SLregexp_match (SLRegexp_Type *compiled_regexp, char *str, unsigned int len);
SL_EXTERN int SLregexp_nth_match (SLRegexp_Type *, unsigned int nth, unsigned int *ofsp, unsigned int *lenp);

SL_EXTERN int SLregexp_get_hints (SLRegexp_Type *, unsigned int *flagsp);
#define SLREGEXP_HINT_BOL		0x01   /* pattern must match bol */
#define SLREGEXP_HINT_OSEARCH		0x02   /* ordinary search will do */

SL_EXTERN char *SLregexp_quote_string (char *, char *, unsigned int);

/*}}}*/

/*{{{ SLang Command Interface */

struct _pSLcmd_Cmd_Type; /* Pre-declaration is needed below */
typedef struct
{
   struct _pSLcmd_Cmd_Type *table;
   int argc;
   /* Version 2.0 needs to use a union!! */
   char **string_args;
   int *int_args;
   double *double_args;
   SLtype *arg_type;
   unsigned long reserved[4];
} SLcmd_Cmd_Table_Type;

typedef struct _pSLcmd_Cmd_Type
{
   int (*cmdfun)(int, SLcmd_Cmd_Table_Type *);
   char *cmd;
   char *arg_type;
} SLcmd_Cmd_Type;

SL_EXTERN int SLcmd_execute_string (char *, SLcmd_Cmd_Table_Type *);

/*}}}*/

/*{{{ SLang Search Interface */

typedef struct _pSLsearch_Type SLsearch_Type;
SL_EXTERN SLsearch_Type *SLsearch_new (SLuchar_Type *u, int search_flags);
#define SLSEARCH_CASELESS	0x1
#define SLSEARCH_UTF8		0x2

SL_EXTERN void SLsearch_delete (SLsearch_Type *);


SL_EXTERN SLuchar_Type *SLsearch_forward (SLsearch_Type *st,
                                        SLuchar_Type *pmin, SLuchar_Type *pmax);
SL_EXTERN SLuchar_Type *SLsearch_backward (SLsearch_Type *st,
                                         SLuchar_Type *pmin, SLuchar_Type *pstart, SLuchar_Type *pmax);
SL_EXTERN unsigned int SLsearch_match_len (SLsearch_Type *);

/*}}}*/

/*{{{ SLang Pathname Interface */

/* These function return pointers to the original space */
SL_EXTERN char *SLpath_basename (char *);
SL_EXTERN char *SLpath_extname (char *);

SL_EXTERN int SLpath_is_absolute_path (char *);

/* Get and set the character delimiter for search paths */
SL_EXTERN int SLpath_get_delimiter (void);
SL_EXTERN int SLpath_set_delimiter (int);

/* search path for loading .sl files */
SL_EXTERN int SLpath_set_load_path (char *);   
/* search path for loading .sl files --- returns slstring */
SL_EXTERN char *SLpath_get_load_path (void);   

/* These return malloced strings--- NOT slstrings */
SL_EXTERN char *SLpath_dircat (char *, char *);
SL_EXTERN char *SLpath_find_file_in_path (char *, char *);
SL_EXTERN char *SLpath_dirname (char *);
SL_EXTERN int SLpath_file_exists (char *);
SL_EXTERN char *SLpath_pathname_sans_extname (char *);

/*}}}*/

SL_EXTERN int SLang_set_module_load_path (char *);

#ifdef __cplusplus
# define SLANG_MODULE(name) \
   extern SL_EXPORT "C" int init_##name##_module_ns (char *); \
   extern SL_EXPORT "C" void deinit_##name##_module (void); \
   extern SL_EXPORT "C" int SLmodule_##name##_api_version; \
   SL_EXPORT int SLmodule_##name##_api_version = SLANG_VERSION
#else
# define SLANG_MODULE(name) \
   extern SL_EXPORT int init_##name##_module_ns (char *); \
   extern SL_EXPORT void deinit_##name##_module (void); \
   SL_EXPORT int SLmodule_##name##_api_version = SLANG_VERSION
#endif

SL_EXTERN int SLvsnprintf (char *, unsigned int, char *, va_list);
SL_EXTERN int SLsnprintf (char *, unsigned int, char *, ...);

#if 0
{
#endif
#ifdef __cplusplus
}
#endif

#endif  /* DAVIS_SLANG_H_ */
