#ifndef _PRIVATE_SLANG_H_
#define _PRIVATE_SLANG_H_
/* header file for S-Lang internal structures that users do not (should not)
   need.  Use slang.h for that purpose. */
/* Copyright (c) 1992, 1999, 2001, 2002, 2003 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

/* #include "config.h" */
#include "jdmacros.h"
#include "sllimits.h"

#ifdef VMS
# define SLANG_SYSTEM_NAME "_VMS"
#else
# if defined (IBMPC_SYSTEM)
#  define SLANG_SYSTEM_NAME "_IBMPC"
# else
#  define SLANG_SYSTEM_NAME "_UNIX"
# endif
#endif  /* VMS */

/* These quantities are main_types for byte-compiled code.  They are used
 * by the inner_interp routine.  The _BC_ means byte-code.
 */

#define _SLANG_BC_LVARIABLE	SLANG_LVARIABLE   /* 0x01 */
#define _SLANG_BC_GVARIABLE	SLANG_GVARIABLE   /* 0x02 */
#define _SLANG_BC_IVARIABLE 	SLANG_IVARIABLE   /* 0x03 */
#define _SLANG_BC_RVARIABLE	SLANG_RVARIABLE   /* 0x04 */
#define _SLANG_BC_INTRINSIC 	SLANG_INTRINSIC   /* 0x05 */
#define _SLANG_BC_FUNCTION  	SLANG_FUNCTION   /* 0x06 */
#define _SLANG_BC_MATH_UNARY	SLANG_MATH_UNARY   /* 0x07 */
#define _SLANG_BC_APP_UNARY	SLANG_APP_UNARY   /* 0x08 */
#define _SLANG_BC_ICONST	SLANG_ICONSTANT   /* 0x09 */
#define _SLANG_BC_DCONST	SLANG_DCONSTANT   /* 0x0A */
#define _SLANG_BC_PVARIABLE	SLANG_PVARIABLE   /* 0x0B */
#define _SLANG_BC_PFUNCTION	SLANG_PFUNCTION   /* 0x0C */

#define _SLANG_BC_UNUSED_0x0D	0x0D
#define _SLANG_BC_UNUSED_0x0E	0x0E
#define _SLANG_BC_UNUSED_0x0F	0x0F
#define _SLANG_BC_BINARY	0x10
#define _SLANG_BC_LITERAL	0x11           /* constant objects */
#define _SLANG_BC_LITERAL_INT	0x12
#define _SLANG_BC_LITERAL_STR	0x13
#define _SLANG_BC_BLOCK		0x14

/* These 3 MUST be in this order too ! */
#define _SLANG_BC_RETURN	0x15
#define _SLANG_BC_BREAK		0x16
#define _SLANG_BC_CONTINUE	0x17

#define _SLANG_BC_EXCH		0x18
#define _SLANG_BC_LABEL		0x19
#define _SLANG_BC_LOBJPTR	0x1A
#define _SLANG_BC_GOBJPTR	0x1B
#define _SLANG_BC_X_ERROR	0x1C
/* These must be in this order */
#define _SLANG_BC_X_USER0	0x1D
#define _SLANG_BC_X_USER1	0x1E
#define _SLANG_BC_X_USER2	0x1F
#define _SLANG_BC_X_USER3	0x20
#define _SLANG_BC_X_USER4	0x21

#define _SLANG_BC_LITERAL_DBL	0x22
#define _SLANG_BC_UNUSED_0x23		

#define _SLANG_BC_CALL_DIRECT		0x24
#define _SLANG_BC_CALL_DIRECT_FRAME	0x25
#define _SLANG_BC_UNARY			0x26
#define _SLANG_BC_UNARY_FUNC		0x27

#define _SLANG_BC_UNUSED_0x28	0x28
#define _SLANG_BC_UNUSED_0x29	0x29
#define _SLANG_BC_UNUSED_0x2A	0x2A
#define _SLANG_BC_UNUSED_0x2B	0x2B
#define _SLANG_BC_UNUSED_0x2C	0x2C
#define _SLANG_BC_UNUSED_0x2D	0x2D
#define _SLANG_BC_UNUSED_0x2E	0x2E
#define _SLANG_BC_UNUSED_0x2F	0x2F

#define _SLANG_BC_DEREF_ASSIGN		0x30
#define _SLANG_BC_SET_LOCAL_LVALUE	0x31
#define _SLANG_BC_SET_GLOBAL_LVALUE	0x32
#define _SLANG_BC_SET_INTRIN_LVALUE	0x33
#define _SLANG_BC_SET_STRUCT_LVALUE	0x34
#define _SLANG_BC_FIELD			0x35
#define _SLANG_BC_SET_ARRAY_LVALUE	0x36

#define _SLANG_BC_UNUSED_0x37	0x37
#define _SLANG_BC_UNUSED_0x38	0x38
#define _SLANG_BC_UNUSED_0x39	0x39
#define _SLANG_BC_UNUSED_0x3A	0x3A
#define _SLANG_BC_UNUSED_0x3B	0x3B
#define _SLANG_BC_UNUSED_0x3C	0x3C
#define _SLANG_BC_UNUSED_0x3D	0x3D
#define _SLANG_BC_UNUSED_0x3E	0x3E
#define _SLANG_BC_UNUSED_0x3F	0x3F

#define _SLANG_BC_LINE_NUM		0x40

#define _SLANG_BC_UNUSED_0x41	0x41
#define _SLANG_BC_UNUSED_0x42	0x42
#define _SLANG_BC_UNUSED_0x43	0x43
#define _SLANG_BC_UNUSED_0x44	0x44
#define _SLANG_BC_UNUSED_0x45	0x45
#define _SLANG_BC_UNUSED_0x46	0x46
#define _SLANG_BC_UNUSED_0x47	0x47
#define _SLANG_BC_UNUSED_0x48	0x48
#define _SLANG_BC_UNUSED_0x49	0x49
#define _SLANG_BC_UNUSED_0x4A	0x4A
#define _SLANG_BC_UNUSED_0x4B	0x4B
#define _SLANG_BC_UNUSED_0x4C	0x4C
#define _SLANG_BC_UNUSED_0x4D	0x4D
#define _SLANG_BC_UNUSED_0x4E	0x4E
#define _SLANG_BC_UNUSED_0x4F	0x4F

#define _SLANG_BC_TMP			0x50

#define _SLANG_BC_UNUSED_0x51	0x51
#define _SLANG_BC_UNUSED_0x52	0x52
#define _SLANG_BC_UNUSED_0x53	0x53
#define _SLANG_BC_UNUSED_0x54	0x54
#define _SLANG_BC_UNUSED_0x55	0x55
#define _SLANG_BC_UNUSED_0x56	0x56
#define _SLANG_BC_UNUSED_0x57	0x57
#define _SLANG_BC_UNUSED_0x58	0x58
#define _SLANG_BC_UNUSED_0x59	0x59
#define _SLANG_BC_UNUSED_0x5A	0x5A
#define _SLANG_BC_UNUSED_0x5B	0x5B
#define _SLANG_BC_UNUSED_0x5C	0x5C
#define _SLANG_BC_UNUSED_0x5D	0x5D
#define _SLANG_BC_UNUSED_0x5E	0x5E
#define _SLANG_BC_UNUSED_0x5F	0x5F


#define _SLANG_BC_LVARIABLE_AGET	0x60
#define _SLANG_BC_LVARIABLE_APUT	0x61
#define _SLANG_BC_INTEGER_PLUS		0x62
#define _SLANG_BC_INTEGER_MINUS		0x63
#define _SLANG_BC_ARG_LVARIABLE		0x64
#define _SLANG_BC_EARG_LVARIABLE	0x65

#define _SLANG_BC_UNUSED_0x66	0x66
#define _SLANG_BC_UNUSED_0x67	0x67
#define _SLANG_BC_UNUSED_0x68	0x68
#define _SLANG_BC_UNUSED_0x69	0x69
#define _SLANG_BC_UNUSED_0x6A	0x6A
#define _SLANG_BC_UNUSED_0x6B	0x6B
#define _SLANG_BC_UNUSED_0x6C	0x6C
#define _SLANG_BC_UNUSED_0x6D	0x6D
#define _SLANG_BC_UNUSED_0x6E	0x6E
#define _SLANG_BC_UNUSED_0x6F	0x6F

#define _SLANG_BC_UNUSED_0x70	0x70
#define _SLANG_BC_UNUSED_0x71	0x71
#define _SLANG_BC_UNUSED_0x72	0x72
#define _SLANG_BC_UNUSED_0x73	0x73
#define _SLANG_BC_UNUSED_0x74	0x74
#define _SLANG_BC_UNUSED_0x75	0x75
#define _SLANG_BC_UNUSED_0x76	0x76
#define _SLANG_BC_UNUSED_0x77	0x77
#define _SLANG_BC_UNUSED_0x78	0x78
#define _SLANG_BC_UNUSED_0x79	0x79
#define _SLANG_BC_UNUSED_0x7A	0x7A
#define _SLANG_BC_UNUSED_0x7B	0x7B
#define _SLANG_BC_UNUSED_0x7C	0x7C
#define _SLANG_BC_UNUSED_0x7D	0x7D
#define _SLANG_BC_UNUSED_0x7E	0x7E
#define _SLANG_BC_UNUSED_0x7F	0x7F

/* These are used only when compiled with USE_COMBINED_BYTECODES */
#define _SLANG_BC_CALL_DIRECT_INTRINSIC	0x80
#define _SLANG_BC_INTRINSIC_CALL_DIRECT	0x81
#define _SLANG_BC_CALL_DIRECT_LSTR	0x82
#define _SLANG_BC_CALL_DIRECT_SLFUN	0x83
#define _SLANG_BC_CALL_DIRECT_INTRSTOP	0x84
#define _SLANG_BC_INTRINSIC_STOP	0x85
#define _SLANG_BC_CALL_DIRECT_EARG_LVAR	0x86
#define _SLANG_BC_CALL_DIRECT_LINT	0x87
#define _SLANG_BC_CALL_DIRECT_LVAR	0x88
#define _SLANG_BC_LLVARIABLE_BINARY	0x89
#define _SLANG_BC_LGVARIABLE_BINARY	0x8A
#define _SLANG_BC_GLVARIABLE_BINARY	0x8B
#define _SLANG_BC_GGVARIABLE_BINARY	0x8C
#define _SLANG_BC_LIVARIABLE_BINARY	0x8D
#define _SLANG_BC_LDVARIABLE_BINARY	0x8E
#define _SLANG_BC_ILVARIABLE_BINARY	0x8F
#define _SLANG_BC_DLVARIABLE_BINARY	0x90
#define _SLANG_BC_LVARIABLE_BINARY	0x91
#define _SLANG_BC_GVARIABLE_BINARY	0x92
#define _SLANG_BC_LITERAL_INT_BINARY	0x93
#define _SLANG_BC_LITERAL_DBL_BINARY	0x94


#define _SLANG_BC_LVARIABLE_COMBINED	0xA0
#define _SLANG_BC_GVARIABLE_COMBINED	0xA1
#define _SLANG_BC_LITERAL_COMBINED	0xA2

/* Byte-Code Sub Types (_BCST_) */

/* These are sub_types of _SLANG_BC_BLOCK */
#define _SLANG_BCST_ERROR_BLOCK	0x01
#define _SLANG_BCST_EXIT_BLOCK	0x02
#define _SLANG_BCST_USER_BLOCK0	0x03
#define _SLANG_BCST_USER_BLOCK1	0x04
#define _SLANG_BCST_USER_BLOCK2	0x05
#define _SLANG_BCST_USER_BLOCK3	0x06
#define _SLANG_BCST_USER_BLOCK4	0x07
/* The user blocks MUST be in the above order */
#define _SLANG_BCST_LOOP	0x10
#define _SLANG_BCST_WHILE	0x11
#define _SLANG_BCST_FOR		0x12
#define _SLANG_BCST_FOREVER	0x13
#define _SLANG_BCST_CFOR	0x14
#define _SLANG_BCST_DOWHILE	0x15
#define _SLANG_BCST_FOREACH	0x16

#define _SLANG_BCST_IF		0x20
#define _SLANG_BCST_IFNOT	0x21
#define _SLANG_BCST_ELSE	0x22
#define _SLANG_BCST_ANDELSE	0x23
#define _SLANG_BCST_ORELSE	0x24
#define _SLANG_BCST_SWITCH	0x25
#define _SLANG_BCST_NOTELSE	0x26

/* assignment (_SLANG_BC_SET_*_LVALUE) subtypes.  The order MUST correspond
 * to the assignment token order with the ASSIGN_TOKEN as the first!
 */
#define _SLANG_BCST_ASSIGN		0x01
#define _SLANG_BCST_PLUSEQS		0x02
#define _SLANG_BCST_MINUSEQS		0x03
#define _SLANG_BCST_TIMESEQS		0x04
#define _SLANG_BCST_DIVEQS		0x05
#define _SLANG_BCST_BOREQS		0x06
#define _SLANG_BCST_BANDEQS		0x07
#define _SLANG_BCST_PLUSPLUS		0x08
#define _SLANG_BCST_POST_PLUSPLUS	0x09
#define _SLANG_BCST_MINUSMINUS		0x0A
#define _SLANG_BCST_POST_MINUSMINUS	0x0B

/* These use SLANG_PLUS, SLANG_MINUS, SLANG_PLUSPLUS, etc... */

typedef union
{
#if SLANG_HAS_FLOAT
   double double_val;
   float float_val;
#endif
   long long_val;
   unsigned long ulong_val;
   VOID_STAR ptr_val;
   char *s_val;
   int int_val;
   unsigned int uint_val;
   SLang_MMT_Type *ref;
   SLang_Name_Type *n_val;
   struct _SLang_Struct_Type *struct_val;
   struct _SLang_Array_Type *array_val;
   short short_val;
   unsigned short ushort_val;
   char char_val;
   unsigned char uchar_val;
}
_SL_Object_Union_Type;

typedef struct _SLang_Object_Type
{
   SLtype data_type;	       /* SLANG_INT_TYPE, ... */
   _SL_Object_Union_Type v;
}
SLang_Object_Type;

struct _SLang_MMT_Type
{
   SLtype data_type;	       /* int, string, etc... */
   VOID_STAR user_data;	       /* address of user structure */
   unsigned int count;		       /* number of references */
};

extern int _SLang_pop_object_of_type (SLtype, SLang_Object_Type *, int);

typedef struct
{
   char *name;			       /* slstring */
   SLang_Object_Type obj;
}
_SLstruct_Field_Type;

typedef struct _SLang_Struct_Type
{
   _SLstruct_Field_Type *fields;
   unsigned int nfields;	       /* number used */
   unsigned int num_refs;
}
_SLang_Struct_Type;

extern void _SLstruct_delete_struct (_SLang_Struct_Type *);
extern int _SLang_push_struct (_SLang_Struct_Type *);
extern int _SLang_pop_struct (_SLang_Struct_Type **);
extern int _SLstruct_init (void);
/* extern int _SLstruct_get_field (char *); */
extern int _SLstruct_define_struct (void);
extern int _SLstruct_define_typedef (void);

struct _SLang_Ref_Type
{
   int is_global;
   union
     {
	SLang_Name_Type *nt;
	SLang_Object_Type *local_obj;
     }
   v;
};

extern int _SLang_dereference_ref (SLang_Ref_Type *);
extern int _SLang_deref_assign (SLang_Ref_Type *);
extern int _SLang_push_ref (int, VOID_STAR);

extern int _SL_increment_frame_pointer (void);
extern int _SL_decrement_frame_pointer (void);

extern int SLang_pop(SLang_Object_Type *);
extern void SLang_free_object (SLang_Object_Type *);
extern int _SLanytype_typecast (SLtype, VOID_STAR, unsigned int,
				SLtype, VOID_STAR);
extern void _SLstring_intrinsic (void);


/* These functions are used to create slstrings of a fixed length.  Be
 * very careful how they are used.  In particular, if len bytes are allocated,
 * then the string must be len characters long, no more and no less.
 */
extern char *_SLallocate_slstring (unsigned int);
extern char *_SLcreate_via_alloced_slstring (char *, unsigned int);
extern void _SLunallocate_slstring (char *, unsigned int);
extern int _SLpush_alloced_slstring (char *, unsigned int);
  
typedef struct 
{
   char **buf;
   unsigned int max_num;
   unsigned int num;
   unsigned int delta_num;
}
_SLString_List_Type;
extern int _SLstring_list_append (_SLString_List_Type *, char *);
extern int _SLstring_list_init (_SLString_List_Type *, unsigned int, unsigned int);
extern void _SLstring_list_delete (_SLString_List_Type *);
extern int _SLstring_list_push (_SLString_List_Type *);

/* This function assumes that s is an slstring. */
extern char *_SLstring_dup_slstring (char *);
extern int _SLang_dup_and_push_slstring (char *);


extern int _SLang_init_import (void);

/* This function checks to see if the referenced object is initialized */
extern int _SLang_is_ref_initialized (SLang_Ref_Type *);
extern int _SLcheck_identifier_syntax (char *);
extern int _SLang_uninitialize_ref (SLang_Ref_Type *);

extern int _SLpush_slang_obj (SLang_Object_Type *);

extern char *_SLexpand_escaped_char(char *, char *);
extern void _SLexpand_escaped_string (char *, char *, char *);

/* returns a pointer to an SLstring string-- use SLang_free_slstring */
extern char *_SLstringize_object (SLang_Object_Type *);
extern int _SLdump_objects (char *, SLang_Object_Type *, unsigned int, int);

extern SLang_Object_Type *_SLang_get_run_stack_pointer (void);
extern SLang_Object_Type *_SLang_get_run_stack_base (void);
extern int _SLang_dump_stack (void);

struct _SLang_NameSpace_Type
{
   struct _SLang_NameSpace_Type *next;
   char *name;			       /* this is the load_type name */
   char *namespace_name;	       /* this name is assigned by implements */
   unsigned int table_size;
   SLang_Name_Type **table;
};
extern SLang_NameSpace_Type *_SLns_allocate_namespace (char *, unsigned int);
extern SLang_NameSpace_Type *_SLns_find_namespace (char *);
extern int _SLns_set_namespace_name (SLang_NameSpace_Type *, char *);
extern SLang_Array_Type *_SLnspace_apropos (SLang_NameSpace_Type *, char *, unsigned int);
extern void _SLang_use_namespace_intrinsic (char *name);
extern char *_SLang_cur_namespace_intrinsic (void);
extern SLang_Array_Type *_SLang_apropos (char *, char *, unsigned int);
extern void _SLang_implements_intrinsic (char *);
extern SLang_Array_Type *_SLns_list_namespaces (void);

extern int _SLang_Trace;
extern int _SLstack_depth(void);
extern char *_SLang_current_function_name (void);

extern int _SLang_trace_fun(char *);
extern int _SLang_Compile_Line_Num_Info;

extern char *_SLstring_dup_hashed_string (char *, unsigned long);
extern unsigned long _SLcompute_string_hash (char *);
extern char *_SLstring_make_hashed_string (char *, unsigned int, unsigned long *);
extern void _SLfree_hashed_string (char *, unsigned int, unsigned long);
unsigned long _SLstring_hash (unsigned char *, unsigned char *);
extern int _SLinit_slcomplex (void);

extern int _SLang_init_slstrops (void);
extern int _SLstrops_do_sprintf_n (int);
extern int _SLang_sscanf (void);
extern double _SLang_atof (char *);
extern int _SLang_init_bstring (void);
extern int _SLang_init_sltime (void);
extern void _SLpack (void);
extern void _SLunpack (char *, SLang_BString_Type *);
extern void _SLpack_pad_format (char *);
extern unsigned int _SLpack_compute_size (char *);
extern int _SLusleep (unsigned long);

/* frees upon error.  NULL __NOT__ ok. */
extern int _SLang_push_slstring (char *);

extern SLtype _SLarith_promote_type (SLtype);
extern int _SLarith_get_precedence (SLtype);
extern int _SLarith_typecast (SLtype, VOID_STAR, unsigned int,
			      SLtype, VOID_STAR);

extern int SLang_push(SLang_Object_Type *);
extern int SLadd_global_variable (char *);
extern void _SLang_clear_error (void);

extern int _SLdo_pop (void);
extern unsigned int _SLsys_getkey (void);
extern int _SLsys_input_pending (int);
#ifdef IBMPC_SYSTEM
extern unsigned int _SLpc_convert_scancode (unsigned int, unsigned int, int);
#define _SLTT_KEY_SHIFT		1
#define _SLTT_KEY_CTRL		2
#define _SLTT_KEY_ALT		4
#endif

typedef struct _SLterminfo_Type SLterminfo_Type;
extern SLterminfo_Type *_SLtt_tigetent (char *);
extern char *_SLtt_tigetstr (SLterminfo_Type *, char *);
extern int _SLtt_tigetnum (SLterminfo_Type *, char *);
extern int _SLtt_tigetflag (SLterminfo_Type *, char *);

#if SLTT_HAS_NON_BCE_SUPPORT
extern int _SLtt_get_bce_color_offset (void);
#endif
extern void (*_SLtt_color_changed_hook)(void);

extern unsigned char SLang_Input_Buffer [SL_MAX_INPUT_BUFFER_LEN];

extern int _SLregister_types (void);
extern SLang_Class_Type *_SLclass_get_class (SLtype);
extern VOID_STAR _SLclass_get_ptr_to_value (SLang_Class_Type *, SLang_Object_Type *);
extern void _SLclass_type_mismatch_error (SLtype, SLtype);
extern int _SLclass_init (void);
extern int _SLclass_copy_class (SLtype, SLtype);

extern int (*_SLclass_get_typecast (SLtype, SLtype, int))
(SLtype, VOID_STAR, unsigned int,
 SLtype, VOID_STAR);

extern int (*_SLclass_get_binary_fun (int, SLang_Class_Type *, SLang_Class_Type *, SLang_Class_Type **, int))
(int,
 SLtype, VOID_STAR, unsigned int,
 SLtype, VOID_STAR, unsigned int,
 VOID_STAR);

extern int (*_SLclass_get_unary_fun (int, SLang_Class_Type *, SLang_Class_Type **, int))
(int, SLtype, VOID_STAR, unsigned int, VOID_STAR);

extern int _SLarith_register_types (void);
extern SLtype _SLarith_Arith_Types [];

extern int _SLang_is_arith_type (SLtype);
extern void _SLang_set_arith_type (SLtype, unsigned char);
#if _SLANG_OPTIMIZE_FOR_SPEED
extern int _SLang_get_class_type (SLtype);
extern void _SLang_set_class_type (SLtype, SLtype);
#endif
extern int _SLarith_bin_op (SLang_Object_Type *, SLang_Object_Type *, int);

extern int _SLarray_add_bin_op (SLtype);

extern int _SLang_call_funptr (SLang_Name_Type *);
extern void _SLset_double_format (char *);
extern SLang_Name_Type *_SLlocate_global_name (char *);
extern SLang_Name_Type *_SLlocate_name (char *);

extern char *_SLdefines[];

#define SL_ERRNO_NOT_IMPLEMENTED	0x7FFF
extern int _SLerrno_errno;
extern int _SLerrno_init (void);

extern int _SLstdio_fdopen (char *, int, char *);

extern void _SLstruct_pop_args (int *);
extern void _SLstruct_push_args (SLang_Array_Type *);

extern int _SLarray_aput (void);
extern int _SLarray_aget (void);
extern int _SLarray_inline_implicit_array (void);
extern int _SLarray_inline_array (void);
extern int _SLarray_wildcard_array (void);

extern int
_SLarray_typecast (SLtype, VOID_STAR, unsigned int,
		   SLtype, VOID_STAR, int);

extern int _SLarray_aput_transfer_elem (SLang_Array_Type *, int *,
					VOID_STAR, unsigned int, int);
extern int _SLarray_aget_transfer_elem (SLang_Array_Type *, int *,
					VOID_STAR, unsigned int, int);
extern void _SLarray_free_array_elements (SLang_Class_Type *, VOID_STAR, unsigned int);

extern SLang_Foreach_Context_Type *
_SLarray_cl_foreach_open (SLtype, unsigned int);
extern void _SLarray_cl_foreach_close (SLtype, SLang_Foreach_Context_Type *);
extern int _SLarray_cl_foreach (SLtype, SLang_Foreach_Context_Type *);

extern int _SLarray_matrix_multiply (void);
extern void (*_SLang_Matrix_Multiply)(void);

extern int _SLarray_next_index (int *, int *, unsigned int);

extern int _SLarray_init_slarray (void);
extern SLang_Array_Type *
SLang_create_array1 (SLtype, int, VOID_STAR, int *, unsigned int, int);

extern int _SLassoc_aput (SLtype, unsigned int);
extern int _SLassoc_aget (SLtype, unsigned int);

extern int _SLcompile_push_context (SLang_Load_Type *);
extern int _SLcompile_pop_context (void);
extern int _SLang_Auto_Declare_Globals;

typedef struct
{
   union
     {
	long long_val;
	char *s_val;		       /* Used for IDENT_TOKEN, FLOAT, etc...  */
	SLang_BString_Type *b_val;
     } v;
   int free_sval_flag;
   unsigned int num_refs;
   unsigned long hash;
#if _SLANG_HAS_DEBUG_CODE
   int line_number;
#endif
   SLtype type;
}
_SLang_Token_Type;

extern void _SLcompile (_SLang_Token_Type *);
extern void (*_SLcompile_ptr)(_SLang_Token_Type *);

/* slmisc.c */
extern char *_SLskip_whitespace (char *s);

/* slospath.c */
extern char *_SLpath_find_file (char *);   /* slstring returned */


/* *** TOKENS *** */

/* Note that that tokens corresponding to ^J, ^M, and ^Z should not be used.
 * This is because a file that contains any of these characters will
 * have an OS dependent interpretation, e.g., ^Z is EOF on MSDOS.
 */

/* Special tokens */
#define ILLEGAL_TOKEN	0x00	       /* no token has this value */
#define EOF_TOKEN	0x01
#define RPN_TOKEN	0x02
#define NL_TOKEN	0x03
#define NOP_TOKEN	0x05
#define FARG_TOKEN	0x06
#define TMP_TOKEN	0x07

#define RESERVED1_TOKEN	0x0A	       /* \n */
#define RESERVED2_TOKEN	0x0D	       /* \r */

/* Literal tokens */
#define CHAR_TOKEN	0x10
#define UCHAR_TOKEN	0x11
#define SHORT_TOKEN	0x12
#define USHORT_TOKEN	0x13
#define INT_TOKEN	0x14
#define UINT_TOKEN	0x15
#define LONG_TOKEN	0x16
#define ULONG_TOKEN	0x17
#define IS_INTEGER_TOKEN(x) ((x >= CHAR_TOKEN) && (x <= ULONG_TOKEN))
#define FLOAT_TOKEN	0x18
#define DOUBLE_TOKEN	0x19
#define RESERVED3_TOKEN	0x1A	       /* ^Z */
#define COMPLEX_TOKEN	0x1B
#define STRING_TOKEN    0x1C
#define BSTRING_TOKEN	0x1D
#define _BSTRING_TOKEN	0x1E	       /* byte-compiled BSTRING */
#define ESC_STRING_TOKEN	0x1F

/* Tokens that can be LVALUES */
#define IDENT_TOKEN	0x20
#define ARRAY_TOKEN	0x21
#define DOT_TOKEN	0x22
#define METHOD_TOKEN	0x24
#define IS_LVALUE_TOKEN (((t) <= METHOD_TOKEN) && ((t) >= IDENT_TOKEN))
/* do not use these values */
#define RESERVED4_TOKEN	0x23	       /* # */
#define RESERVED5_TOKEN 0x25	       /* % */

/* Flags for struct fields */
#define STATIC_TOKEN	0x26
#define READONLY_TOKEN	0x27
#define PRIVATE_TOKEN	0x28
#define PUBLIC_TOKEN	0x29

/* Punctuation tokens */
#define OBRACKET_TOKEN	0x2a
#define CBRACKET_TOKEN	0x2b
#define OPAREN_TOKEN	0x2c
#define CPAREN_TOKEN	0x2d
#define OBRACE_TOKEN	0x2e
#define CBRACE_TOKEN	0x2f

#define COMMA_TOKEN	0x31
#define SEMICOLON_TOKEN	0x32
#define COLON_TOKEN	0x33
#define NAMESPACE_TOKEN	0x34

/* Operators */
#define POW_TOKEN	0x38

/* The order here must match the order in the Binop_Level table in slparse.c */
#define FIRST_BINARY_OP	0x39
#define ADD_TOKEN	0x39
#define SUB_TOKEN	0x3a
#define TIMES_TOKEN	0x3b
#define DIV_TOKEN	0x3c
#define LT_TOKEN	0x3d
#define LE_TOKEN	0x3e
#define GT_TOKEN	0x3f
#define GE_TOKEN	0x40
#define EQ_TOKEN	0x41
#define NE_TOKEN	0x42
#define AND_TOKEN	0x43
#define OR_TOKEN	0x44
#define MOD_TOKEN	0x45
#define BAND_TOKEN	0x46
#define SHL_TOKEN	0x47
#define SHR_TOKEN	0x48
#define BXOR_TOKEN	0x49
#define BOR_TOKEN	0x4a
#define POUND_TOKEN	0x4b	       /* matrix multiplication */

#define LAST_BINARY_OP	 0x4b
#define IS_BINARY_OP(t)	 ((t >= FIRST_BINARY_OP) && (t <= LAST_BINARY_OP))

/* unary tokens -- but not all of them (see grammar) */
#define DEREF_TOKEN	 0x4d
#define NOT_TOKEN	 0x4e
#define BNOT_TOKEN	 0x4f

#define IS_INTERNAL_FUNC(t)	((t >= 0x50) && (t <= 0x56))
#define POP_TOKEN	 0x50
#define CHS_TOKEN	 0x51
#define SIGN_TOKEN	 0x52
#define ABS_TOKEN	 0x53
#define SQR_TOKEN	 0x54
#define MUL2_TOKEN	 0x55
#define EXCH_TOKEN	 0x56

/* Assignment tokens.  Note: these must appear with sequential values.
 * The order here must match the specific lvalue assignments below.
 * These tokens are used by rpn routines in slang.c.  slparse.c maps them
 * onto the specific lvalue tokens while parsing infix.
 * Also the assignment _SLANG_BCST_ assumes this order
 */
#define ASSIGN_TOKEN		0x57
#define PLUSEQS_TOKEN	 	0x58
#define MINUSEQS_TOKEN		0x59
#define TIMESEQS_TOKEN		0x5A
#define DIVEQS_TOKEN		0x5B
#define BOREQS_TOKEN		0x5C
#define BANDEQS_TOKEN		0x5D
#define PLUSPLUS_TOKEN		0x5E
#define POST_PLUSPLUS_TOKEN	0x5F
#define MINUSMINUS_TOKEN	0x60
#define POST_MINUSMINUS_TOKEN	0x61

/* Directives */
#define FIRST_DIRECTIVE_TOKEN	0x62
#define IFNOT_TOKEN	0x62
#define IF_TOKEN	0x63
#define ELSE_TOKEN	0x64
#define FOREVER_TOKEN	0x65
#define WHILE_TOKEN	0x66
#define FOR_TOKEN	0x67
#define _FOR_TOKEN	0x68
#define LOOP_TOKEN	0x69
#define SWITCH_TOKEN	0x6A
#define DOWHILE_TOKEN	0x6B
#define ANDELSE_TOKEN	0x6C
#define ORELSE_TOKEN	0x6D
#define ERRBLK_TOKEN	0x6E
#define EXITBLK_TOKEN	0x6F
/* These must be sequential */
#define USRBLK0_TOKEN	0x70
#define USRBLK1_TOKEN	0x71
#define USRBLK2_TOKEN	0x72
#define USRBLK3_TOKEN	0x73
#define USRBLK4_TOKEN	0x74

#define CONT_TOKEN	0x75
#define BREAK_TOKEN	0x76
#define RETURN_TOKEN	0x77

#define CASE_TOKEN	0x78
#define DEFINE_TOKEN	0x79
#define DO_TOKEN	0x7a
#define VARIABLE_TOKEN	0x7b
#define GVARIABLE_TOKEN	0x7c
#define _REF_TOKEN	0x7d
#define PUSH_TOKEN	0x7e
#define STRUCT_TOKEN	0x7f
#define TYPEDEF_TOKEN	0x80
#define NOTELSE_TOKEN	0x81
#define DEFINE_STATIC_TOKEN	0x82
#define FOREACH_TOKEN	0x83
#define USING_TOKEN	0x84
#define DEFINE_PRIVATE_TOKEN	0x85
#define DEFINE_PUBLIC_TOKEN	0x86

/* Note: the order here must match the order of the generic assignment tokens.
 * Also, the first token of each group must be the ?_ASSIGN_TOKEN.
 * slparse.c exploits this order, as well as slang.h.
 */
#define FIRST_ASSIGN_TOKEN		0x90
#define _STRUCT_ASSIGN_TOKEN		0x90
#define _STRUCT_PLUSEQS_TOKEN		0x91
#define _STRUCT_MINUSEQS_TOKEN		0x92
#define _STRUCT_TIMESEQS_TOKEN		0x93
#define _STRUCT_DIVEQS_TOKEN		0x94
#define _STRUCT_BOREQS_TOKEN		0x95
#define _STRUCT_BANDEQS_TOKEN		0x96
#define _STRUCT_PLUSPLUS_TOKEN		0x97
#define _STRUCT_POST_PLUSPLUS_TOKEN	0x98
#define _STRUCT_MINUSMINUS_TOKEN	0x99
#define _STRUCT_POST_MINUSMINUS_TOKEN	0x9A

#define _ARRAY_ASSIGN_TOKEN		0xA0
#define _ARRAY_PLUSEQS_TOKEN		0xA1
#define _ARRAY_MINUSEQS_TOKEN		0xA2
#define _ARRAY_TIMESEQS_TOKEN		0xA3
#define _ARRAY_DIVEQS_TOKEN		0xA4
#define _ARRAY_BOREQS_TOKEN		0xA5
#define _ARRAY_BANDEQS_TOKEN		0xA6
#define _ARRAY_PLUSPLUS_TOKEN		0xA7
#define _ARRAY_POST_PLUSPLUS_TOKEN	0xA8
#define _ARRAY_MINUSMINUS_TOKEN		0xA9
#define _ARRAY_POST_MINUSMINUS_TOKEN	0xAA

#define _SCALAR_ASSIGN_TOKEN		0xB0
#define _SCALAR_PLUSEQS_TOKEN		0xB1
#define _SCALAR_MINUSEQS_TOKEN		0xB2
#define _SCALAR_TIMESEQS_TOKEN		0xB3
#define _SCALAR_DIVEQS_TOKEN		0xB4
#define _SCALAR_BOREQS_TOKEN		0xB5
#define _SCALAR_BANDEQS_TOKEN		0xB6
#define _SCALAR_PLUSPLUS_TOKEN		0xB7
#define _SCALAR_POST_PLUSPLUS_TOKEN	0xB8
#define _SCALAR_MINUSMINUS_TOKEN	0xB9
#define _SCALAR_POST_MINUSMINUS_TOKEN	0xBA

#define _DEREF_ASSIGN_TOKEN		0xC0
#define _DEREF_PLUSEQS_TOKEN		0xC1
#define _DEREF_MINUSEQS_TOKEN		0xC2
#define _DEREF_TIMESEQS_TOKEN		0xC3
#define _DEREF_DIVEQS_TOKEN		0xC4
#define _DEREF_BOREQS_TOKEN		0xC5
#define _DEREF_BANDEQS_TOKEN		0xC6
#define _DEREF_PLUSPLUS_TOKEN		0xC7
#define _DEREF_POST_PLUSPLUS_TOKEN	0xC8
#define _DEREF_MINUSMINUS_TOKEN		0xC9
#define _DEREF_POST_MINUSMINUS_TOKEN	0xCA

#define LAST_ASSIGN_TOKEN		0xCA
#define IS_ASSIGN_TOKEN(t) (((t)>=FIRST_ASSIGN_TOKEN)&&((t)<=LAST_ASSIGN_TOKEN))

#define _INLINE_ARRAY_TOKEN		0xE0
#define _INLINE_IMPLICIT_ARRAY_TOKEN	0xE1
#define _NULL_TOKEN			0xE2
#define _INLINE_WILDCARD_ARRAY_TOKEN	0xE3

#define LINE_NUM_TOKEN			0xFC
#define ARG_TOKEN	 		0xFD
#define EARG_TOKEN	 		0xFE
#define NO_OP_LITERAL			0xFF

typedef struct
{
   /* sltoken.c */
   /* SLang_eval_object */
   SLang_Load_Type *llt;
   SLPreprocess_Type *this_slpp;
   /* prep_get_char() */
   char *input_line;
   char cchar;
   /* get_token() */
   int want_nl_token;

   /* slparse.c */
   _SLang_Token_Type ctok;
   int block_depth;
   int assignment_expression;

   /* slang.c : SLcompile() */
   _SLang_Token_Type save_token;
   _SLang_Token_Type next_token;
   void (*slcompile_ptr)(_SLang_Token_Type *);
}
_SLEval_Context;

extern int _SLget_token (_SLang_Token_Type *);
extern void _SLparse_error (char *, _SLang_Token_Type *, int);
extern void _SLparse_start (SLang_Load_Type *);
extern int _SLget_rpn_token (_SLang_Token_Type *);
extern void _SLcompile_byte_compiled (void);

extern int (*_SLprep_eval_hook) (char *);

/* GNU Midnight Commander uses replacements from glib */
#define MIDNIGHT_COMMANDER_CODE 1
#ifndef MIDNIGHT_COMMANDER_CODE

#ifdef HAVE_VSNPRINTF
#define _SLvsnprintf vsnprintf
#else
extern int _SLvsnprintf (char *, unsigned int, char *, va_list);
#endif

#ifdef HAVE_SNPRINTF
# define _SLsnprintf snprintf
#else
extern int _SLsnprintf (char *, unsigned int, char *, ...);
#endif

#endif                                /* !MIDNIGHT_COMMANDER_CODE */
extern int _SLsecure_issetugid (void);
extern char *_SLsecure_getenv (char *);

#undef _INLINE_
#if defined(__GNUC__) && _SLANG_USE_INLINE_CODE
# define _INLINE_ __inline__
#else
# define _INLINE_
#endif


#endif				       /* _PRIVATE_SLANG_H_ */
