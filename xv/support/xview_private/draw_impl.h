/*	@(#)draw_impl.h 20.33 93/06/28 SMI	*/

/***********************************************************************/
/*	                      draw_impl.h			       */
/*	
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL NOTICE 
 *	file for terms of the license. 
 */
/***********************************************************************/

#ifndef _xview_private_drawable_impl_h_already_defined
#define _xview_private_drawable_impl_h_already_defined

#include <xview_private/portable.h>
#include <xview/pkg.h>
#include <xview/drawable.h>
#include <xview/cms.h>
#include <xview_private/scrn_vis.h>


/* Although this is a private implementation header file, it is included
 * by most of the Drawable sub-pkgs for performance reasons.
 */
#define drawable_attr_next(attr) \
	(Drawable_attribute *)attr_next((Xv_opaque *)attr)

#define	DRAWABLE_PRIVATE(drawable) \
			XV_PRIVATE(Xv_Drawable_info, Xv_drawable_struct, drawable)
/* Note: Xv_Drawable_info has no public_self field, so XV_PUBLIC cannot work
 * and DRAWABLE_PUBLIC is undefined.
 */


/***********************************************************************/
/*	        	Structures 				       */
/***********************************************************************/

typedef struct drawable_info {
    XID			 xid;
    unsigned long	 fg;
    unsigned long	 bg;
    Cms		 	 cms;
    int			 cms_fg;
    int			 cms_bg;
    unsigned long	 plane_mask;
    Screen_visual	*visual;
	/* Flags */
    unsigned		 private_gc	: 1;	/* Should be gc itself? */
    unsigned		 no_focus	: 1;	/* Don't set focus on click */
    unsigned		 has_focus	: 1;	/* Currently has focus */
    unsigned		 new_clipping	: 1;	/* new clipping has been set*/
    unsigned		 dynamic_color  : 1;    /* created with dynamic cmap*/
    unsigned		 is_bitmap	: 1;	/* use 1 and 0 as only colors */
} Xv_Drawable_info;

#define	xv_xid(info)		((info)->xid)
#define xv_fg(info)		((info)->fg)
#define xv_bg(info)		((info)->bg)
#define	xv_cms(info)		((info)->cms)
#define xv_cms_fg(info)		((info)->cms_fg)
#define xv_cms_bg(info)		((info)->cms_bg)
#define xv_plane_mask(info)	((info)->plane_mask)
#define xv_visual(info)		((info)->visual)
#define	xv_display(info)	((info)->visual->display)
#define	xv_server(info)		((info)->visual->server)
#define	xv_screen(info)		((info)->visual->screen)
#define	xv_root(info)		((info)->visual->root_window)
#define	xv_depth(info)		((info)->visual->depth)
#define	xv_image_bitmap(info)	((info)->visual->image_bitmap)
#define	xv_image_pixmap(info)	((info)->visual->image_pixmap)
#define xv_dynamic_color(info)  ((info)->dynamic_color)
#define xv_is_bitmap(info)	((info)->is_bitmap)

#define	xv_gc(public, info) \
	((info)->private_gc ? xv_private_gc((public)) : (info)->visual->gc)

#define	xv_set_image(info, im)	(info)->visual->image = im

#define xv_no_focus(info) ((info)->no_focus)
#define xv_set_no_focus(info, f) (info)->no_focus = (f)

#define xv_has_focus(info) ((info)->has_focus)
#define xv_set_has_focus(info, f) (info)->has_focus = (f)

extern const char *xv_draw_info_str;
#define DRAWABLE_INFO_MACRO(_win_public, _info)\
{\
   if (_win_public) {\
      Xv_opaque  _object;\
      XV_OBJECT_TO_STANDARD(_win_public, xv_draw_info_str, _object);\
      _info = (_object ? ((Xv_Drawable_info *)(((Xv_drawable_struct *)(_object))->private_data)) : 0);\
   } else _info = 0;\
}
        
extern GC		xv_private_gc();
extern Xv_Drawable_info	*drawable_info();
/* drawable.c */
Pkg_private Xv_opaque        drawable_get_attr();
Pkg_private int              drawable_init();
Pkg_private int              drawable_destroy();

#endif
