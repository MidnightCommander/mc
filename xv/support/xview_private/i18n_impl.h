/*      @(#)i18n_impl.h 1.23 93/06/28 SMI	*/
/*
 *      (c) Copyright 1990 Sun Microsystems, Inc. Sun design patents 
 *      pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *      file for terms of the license.
 */

#ifndef i18n_impl_h_DEFINED
#define i18n_impl_h_DEFINED

#include <xview/xv_c_types.h>
#include <xview/pkg.h>

#ifndef OS_HAS_LOCALE
#define OS_HAS_LOCALE
#endif

#if defined(SVR4) && defined(OW_I18N)

#include <stdlib.h>

#endif /* SVR4 && OW_I18N */

#ifdef OS_HAS_LOCALE

#include  <locale.h>

/* Linux: gcc 2.4.x does not have LC_MESSAGES, but it has LC_RESPONSE instead */
#if defined(__linux) && !defined(LC_MESSAGES) && defined(LC_RESPONSE)
#define LC_MESSAGES LC_RESPONSE
#endif

extern char	*dgettext();

#define XV_I18N_MSG(d,s)	(dgettext(d,s))

#ifndef XV_14_CHARS_FILE_NAME
/*
 * System with long file name.
 */
#define XV_TEXT_DOMAIN          "SUNW_WST_LIBXVIEW"
#else /* XV_14_CHARS_FILE_NAME */
/*
 * System with short (max 14 chars) file name.
 */
#define XV_TEXT_DOMAIN          "XVIEW"
#endif /* XV_14_CHARS_FILE_NAME */

#ifdef XGETTEXT
#define	xv_domain		XV_TEXT_DOMAIN
#else  /* XGETTEXT */
/* Initial value assigned at xv_init.c */
Xv_private_data CONST char	*xv_domain;
#endif /* XGETTEXT */

#define XV_MSG(s)		(dgettext(xv_domain, s))

#else  /* OS_HAS_LOCALE */

#define XV_I18N_MSG(d,s)	((s))

#define XV_MSG(s)		((s))

#endif /* OS_HAS_LOCALE */

#ifdef OW_I18N
/*
 * This is only for the level four i18n (ie, supporting the Asian
 * locales).
 */

#include <xview/xv_i18n.h>
#include <X11/Xutil.h>

/*
 * Those macros has been defined to reduce the many of "#ifdef
 * OW_I18N" inside the source code.  Using those macros should improve
 * the readability of the source code.
 */
#define	ATOI		watoi
#define	CHAR		wchar_t
#define	INDEX		STRCHR
#define	RINDEX		STRRCHR
#define	SPRINTF		wsprintf
#define	STRCAT		wscat
#define	STRCHR		wschr
#define	STRCMP		wscmp
#define	STRCPY		wscpy
#ifdef notdef
/* Conflict with sun.h's define */
#define	STRDUP		wsdup
#endif
#define	STRLEN		wslen
#define	STRNCAT		wsncat
#define	STRNCMP		wsncmp
#define	STRNCPY		wsncpy
#define	STRRCHR		wsrchr

/*
 * DEPEND_ON_OS: word selection library, the following _wckind is
 * tentative solution until SunOS officially provides to us.
 * wchar_type is for transition from previous implementation, should
 * use wchar_kind interface instead.
 */
#define wchar_type(wp)	_wckind(*(wp))
#define wchar_kind(wc)	_wckind(wc)

/*
 * FIX_ME: Just for tentative, should be removed before FCS.
 */
#define	mbstowcsdup	_xv_mbstowcsdup
#define	wcstombsdup	_xv_wcstombsdup
#define	ctstowcsdup	_xv_ctstowcsdup
#define	wcstoctsdup	_xv_wcstoctsdup


/*
 * To reduce number of malloc call, but same time maintain flexibility
 * to hold arbitrary number of character, here to define new structure
 * called Pseudo Static String.  This is internal for the XView.
 */
typedef struct _xv_pswcs {
    int		 length;
    wchar_t	*storage;	/* Actual malloced storage area */
    wchar_t	*value;		/* Keeps value, free set the value */
} _xv_pswcs_t;

typedef struct _xv_psmbs {
    int		 length;
    char	*storage;	/* Actual malloced storage area */
    char	*value;		/* Keeps value, free set the value */
} _xv_psmbs_t;


/*
 * To support multibyte string attribute which should not dup/copy in
 * uniform way, following structure and set of associated functions
 * must be used.
 */
typedef struct _xv_string_attr_nodup {
    enum {
	XSAN_NOT_SET	= 0,
	XSAN_SET_BY_MBS = 1,
	XSAN_SET_BY_WCS = 2
    }			 flag;
    _xv_psmbs_t		 psmbs;
    _xv_pswcs_t		 pswcs;
} _xv_string_attr_nodup_t;

/*
 * Now this for string attribute which does copy/dup.
 */
typedef struct _xv_string_attr_dup {
    _xv_psmbs_t		 psmbs;
    _xv_pswcs_t		 pswcs;
} _xv_string_attr_dup_t;


/*
 * Character classfication function (should replace by the what OS
 * provides).
 */
EXTERN_FUNCTION (int _wckind, (wchar_t wc));

/*
 * Character convert and dup functions.
 */
EXTERN_FUNCTION (wchar_t *_xv_mbstowcsdup, (char *mbs));
EXTERN_FUNCTION (char    *_xv_wcstombsdup, (wchar_t *wcs));
EXTERN_FUNCTION (wchar_t *_xv_ctstowcsdup, (char *mbs));
EXTERN_FUNCTION (char    *_xv_wcstoctsdup, (wchar_t *wcs));

/*
 * Pseudo Static Wide/MultiByte Character String support routines.
 */
EXTERN_FUNCTION (void _xv_pswcs_wcsdup,	(_xv_pswcs_t *pswcs, wchar_t *new));
EXTERN_FUNCTION (void _xv_pswcs_mbsdup,	(_xv_pswcs_t *pswcs, char *new));
EXTERN_FUNCTION (void _xv_psmbs_wcsdup,	(_xv_psmbs_t *psmbs, wchar_t *new));
EXTERN_FUNCTION (void _xv_psmbs_mbsdup,	(_xv_psmbs_t *psmbs, char *new));

/*
 * No copy/dup string attribute support routines.
 */
#define	_xv_is_string_attr_exist_nodup(xsan) ((xsan)->flag != XSAN_NOT_SET)
EXTERN_FUNCTION (void _xv_use_pswcs_value_nodup,
					(_xv_string_attr_nodup_t *xsan));
EXTERN_FUNCTION (void _xv_use_psmbs_value_nodup,
					(_xv_string_attr_nodup_t *xsan));
EXTERN_FUNCTION (void _xv_set_mbs_attr_nodup,
					(_xv_string_attr_nodup_t *xsan,
					 char *new));
EXTERN_FUNCTION (void _xv_set_wcs_attr_nodup,
					(_xv_string_attr_nodup_t *xsan,
					 wchar_t *new));
EXTERN_FUNCTION (char *_xv_get_mbs_attr_nodup,
					(_xv_string_attr_nodup_t *xsan));
EXTERN_FUNCTION (wchar_t *_xv_get_wcs_attr_nodup,
					(_xv_string_attr_nodup_t *xsan));
EXTERN_FUNCTION (void _xv_free_string_attr_nodup,
					(_xv_string_attr_nodup_t *xsan));
EXTERN_FUNCTION (void _xv_free_ps_string_attr_nodup,
					(_xv_string_attr_nodup_t *xsan));

/*
 * Support routines for string attributes which does copy/duplicate.
 */
#define	_xv_is_string_attr_exist_dup(xsad) \
			((xsad)->psmbs.value || (xsad)->pswcs.value)
EXTERN_FUNCTION (void _xv_set_mbs_attr_dup,
					(_xv_string_attr_dup_t *xsad,
					 char *mbs));
EXTERN_FUNCTION (void _xv_set_wcs_attr_dup,
					(_xv_string_attr_dup_t *xsad,
					 wchar_t *wcs));
EXTERN_FUNCTION (char *_xv_get_mbs_attr_dup,
					(_xv_string_attr_dup_t *xsad));
EXTERN_FUNCTION (wchar_t *_xv_get_wcs_attr_dup,
					(_xv_string_attr_dup_t *xsad));
EXTERN_FUNCTION (void _xv_free_ps_string_attr_dup,
					(_xv_string_attr_dup_t *xsad));

EXTERN_FUNCTION (int _xv_XwcTextListToTextProperty,
					(Xv_object obj, Xv_pkg *pkg,
					 Display *dpy, wchar_t **list,
					 int count,
					 XICCEncodingStyle style,
					 XTextProperty *text_prop));

EXTERN_FUNCTION (int _xv_XwcTextPropertyToTextList,
					(Xv_object obj, Xv_pkg *pkg,
					 Display *dpy,
					 XTextProperty *text_prop,
					 wchar_t ***list, int *count));
#else /* OW_I18N */

#define	ATOI		atoi
#define	CHAR		char
#define	INDEX		STRCHR
#define	RINDEX		STRRCHR
#define	SPRINTF		sprintf
#define	STRCAT		strcat
#define	STRCHR		strchr
#define	STRCMP		strcmp
#define	STRCPY		strcpy
#ifdef notdef
/* Conflict with sun.h's define */
#define	STRDUP		strdup
#endif
#define	STRLEN		strlen
#define	STRNCAT		strncat
#define	STRNCMP		strncmp
#define	STRNCPY		strncpy
#define	STRRCHR		strrchr

#endif /* OW_I18N */

#define XV_STRSAVE(s)	 \
	STRCPY((CHAR *)xv_malloc((STRLEN(s)+1) * sizeof(CHAR)), (s))

#endif /* i18n_impl_h_DEFINED */
