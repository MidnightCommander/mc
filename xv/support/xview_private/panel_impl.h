/*	@(#)panel_impl.h 20.90 93/06/28 SMI	*/

/*
 *	(c) Copyright 1989 Sun Microsystems, Inc. Sun design patents 
 *	pending in the U.S. and foreign countries. See LEGAL_NOTICE 
 *	file for terms of the license.
 */

#ifndef panel_impl_defined
#define panel_impl_defined

#ifndef FILE
#if !defined(SVR4) && !defined(__linux)
#undef NULL
#endif SVR4
#include <stdio.h>
#endif FILE
#include <sys/types.h>
#include <X11/Xlib.h>
#include <xview_private/i18n_impl.h>
#include <xview_private/portable.h>
#include <olgx/olgx.h>
#ifdef OW_I18N
#include <xview/xv_i18n.h>
#endif /* OW_I18N */
#include <xview/pkg.h>
#include <xview/attrol.h>
#include <xview/font.h>
#include <xview/frame.h>
#include <xview/openwin.h>
#include <xview/panel.h>
#include <xview/sel_pkg.h>
#include <xview/svrimage.h>
#include <xview/dragdrop.h>
#include <xview_private/item_impl.h>

/* panels and panel_items are both of type Xv_panel_or_item so that we
 * can pass them to common routines.
 */
#define PANEL_PRIVATE(p)	XV_PRIVATE(Panel_info, Xv_panel, p)
#define PANEL_PUBLIC(panel)     XV_PUBLIC(panel)

#define rect_copy(to, from)		to.r_left = from.r_left; \
                                to.r_top = from.r_top;   \
                                to.r_width = from.r_width; \
                                to.r_height = from.r_height; 
                                
#define	set(value)	((value) != -1) 


#define PANEL_MORE_TEXT_WIDTH 16
#define PANEL_MORE_TEXT_HEIGHT 14
#define PANEL_PULLDOWN_MENU_WIDTH 16
#define PANEL_PULLDOWN_MENU_HEIGHT 8

#define PANEL_SEL_PRIMARY	0
#define PANEL_SEL_SECONDARY	1
#define PANEL_SEL_CLIPBOARD	2
#define NBR_PANEL_SELECTIONS	3


/* 			structures                                      */


/***************************** panel **************************************/
/* *** NOTE: The first three fields of the panel_info struct must match those
 * of the item_info struct, since these are used interchangably in some
 * routines.
 */
typedef struct panel_info {
    /****  DO NOT CHANGE THE ORDER OR PLACEMENT OF THESE THREE FIELDS ****/
    Panel_ops		ops;		/* panel specific operations */
    unsigned int	flags;		/* default item flags */
	/*  N.B.:  panel->flags uses the "Item status flags" definitions
	 * found in "item_impl.h". */
    Panel		public_self;	/* back pointer to object */
    /**** ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ****/

    int			active_caret_ascent;
    int			active_caret_height;
    int			active_caret_width;
    struct {
	Atom		delete;
	Atom		length;
	Atom		null;
	Atom		secondary;
	Atom		selection_end;
	Atom		seln_yield;
#ifdef OW_I18N
	Atom		compound_text;
	Atom		length_chars;
#endif /*OW_I18N*/
    } atom;
    Xv_Font		bold_font;
#ifdef OW_I18N
    XFontSet		bold_fontset_id;
#else
    Font		bold_font_xid;
#endif /* OW_I18N */
    int			caret;		/* current caret character index */
    int			caret_ascent;	/* # of pixels above baseline */
    Pixmap		caret_bg_pixmap; /* used to restore the pixels
					  * underneath the caret */
    int			caret_height;
    int			caret_on;	/* caret is painted */
    int			caret_width;
    Xv_opaque		client_data;	/* for client use */
    Item_info		*current;
    int			current_col_x;	/* left position of current column */
    Cursor		cursor;		/* panel's default (basic) cursor */
    Panel_item		default_item;
    Item_info		*default_drop_site_item;
    int			drag_threshold;
	/* # of pixels the mouse may move with a button down and still be
	 * considered a click.  If it moves more than this, it is considered
	 * to be a press-drag-release gesture instead of a click.  Drag-and-drop
	 * is initiated after the mouse is dragged 'drag_threshold' pixels.
	 * Default is 5.
	 */
    int			duplicate_shiftmask;
	/* Event shiftmask for DUPLICATE modifier key. Default is CTRLMASK. */
    int			(*event_proc)();
    int			extra_height;
    int			extra_width;
    Xv_Window		focus_pw;
	/* Current or last input focus paint window.  (The
	 * panel->status.has_input_focus flag indicates whether
	 * the panel currently has the input focus.) Initially,
	 * focus_pw is set to the first paint window created.
	 */
    Graphics_info	*ginfo;		/* olgx graphics information */
    int			h_margin;	/* horizontal margin */
    int			inactive_caret_ascent;
    int			inactive_caret_height;
    int			inactive_caret_width;
    int			item_x;
    int			item_x_offset;
    int			item_y;
    int			item_y_offset;
    Item_info		*items;
    Item_info		*last_item;
    Item_info		*kbd_focus_item;/* panel item with the keyboard focus */
    Panel_setting	layout;		/* HORIZONTAL, VERTICAL */
    int			(*layout_proc)(); /* interposed window layout proc */
    int			lowest_bottom;	/* lowest bottom of any item */
    int			max_item_y;	/* lowest item on row ??? */
    int			multiclick_timeout;  /* in msec */
    int			no_redisplay_item; /* Don't call panel_redisplay_item
					    * from item_set_generic */
    Panel_paint_window	*paint_window;
    Item_info		*primary_focus_item; /* current or last panel item
					      * that is/was a primary
					      * (First-Class) focus item */
    Panel_setting	repaint;	/* default repaint behavior  */
    int			(*repaint_proc)();
    int			rightmost_right; /* rightmost right of any item */
    Item_info		*sel_holder[NBR_PANEL_SELECTIONS];
#ifdef OW_I18N
    _xv_pswcs_t         clipboard;	  /* none sel_item version of clipb */
#else
    Selection_item	sel_item[NBR_PANEL_SELECTIONS];
#endif
    Selection_owner	sel_owner[NBR_PANEL_SELECTIONS];
    Atom		sel_rank[NBR_PANEL_SELECTIONS];
    Selection_requestor	sel_req;
    Panel_status	status;
    Xv_Font		std_font;	/* standard font */
#ifdef OW_I18N
    XFontSet		std_fontset_id;
#else
    Font		std_font_xid;
#endif /* OW_I18N */
    struct itimerval	timer_full;	/* initial secs & usecs */
    int			v_margin;	/* vertical margin */
#ifdef OW_I18N
    XIC                  ic;
    Item_info		 *preedit_item; /* panel item with the keyboard focus */
    XIMPreeditDrawCallbackStruct  *preedit; /*Save full preedit information*/
    Bool		 preedit_own_by_others;
		/*
		 * When panel text being used by canvas for preedit
		 * rendering, preedit data structure is owned by the
		 * canvas, not panel. So that, panel should not free
		 * the data structure upon destory (note, however,
		 * panel still create the preedit data structure once
		 * created, first canvas in the given frame will use
		 * that data structure, see cnvs_view.c,
		 * canvas_paint_set, XV_END_CREATE).
		 */
#ifdef FULL_R5
    XIMStyle		xim_style;
#endif /* FULL_R5 */
#endif /* OW_I18N */
    int			show_border;
    unsigned short	old_width;
    unsigned short	old_height;
} Panel_info;


#define	PANEL_EACH_PAINT_WINDOW(panel, pw)	\
   {Panel_paint_window *_next; \
    for (_next = (panel)->paint_window; _next != NULL; _next = _next->next) { \
    	(pw) = _next->pw;

#define	PANEL_END_EACH_PAINT_WINDOW	}}

/***********************************************************************/
/* external declarations of private variables                          */
/***********************************************************************/

Pkg_private Attr_attribute	panel_context_key;
Pkg_private Attr_attribute	panel_pw_context_key;


/***********************************************************************/
/* Pkg_private declarations of private functions                          */
/***********************************************************************/

Pkg_private void		panel_accept_kbd_focus();
Pkg_private void              	panel_adjust_label_size();
Pkg_private void		panel_append();
Pkg_private void		panel_btn_accepted();
Pkg_private void		panel_check_item_layout();
Pkg_private void          	panel_clear_item();
Pkg_private void		panel_clear_pw_rect();
Pkg_private void		panel_clear_rect();
Pkg_private int		 	panel_col_to_x();
Pkg_private void		panel_display();
Pkg_private Notify_value	panel_default_event();
Pkg_private int			panel_duplicate_key_is_down();
Pkg_private Rect		panel_enclosing_rect();
Pkg_private int			panel_erase_action();
Pkg_private void		panel_find_default_xy();
Pkg_private int 		panel_fonthome();
Pkg_private void		panel_free_image();
Pkg_private Xv_opaque	 	panel_get_attr();
Pkg_private int			panel_height();
Pkg_private void                panel_image_set_font();
Pkg_private void		panel_invert();
Pkg_private void		panel_invert_polygon();
Pkg_private int			panel_is_multiclick();
Pkg_private void		panel_item_layout();
Pkg_private void		panel_itimer_set();
Pkg_private struct pr_size 	panel_make_image();
Pkg_private int			panel_navigation_action();
Pkg_private Item_info	       *panel_next_kbd_focus();
Pkg_private int		        panel_normalize_scroll();
Pkg_private Notify_value	panel_notify_event();
Pkg_private Notify_value	panel_notify_panel_event();
Pkg_private Notify_value	panel_notify_view_event();
Pkg_private int 		panel_nullproc();
Pkg_private void		panel_paint_image();
Pkg_private void		panel_paint_svrim();
Pkg_private void		panel_paint_text();
Pkg_private Item_info	       *panel_previous_kbd_focus();
Pkg_private int			panel_printable_char();
Pkg_private void		panel_pw_invert();
Pkg_private void		panel_redisplay();
Pkg_private void		panel_redisplay_item();
Pkg_private void		panel_refont();
Pkg_private void		panel_register_view();
Pkg_private int			panel_resize();
Pkg_private int			panel_round();
Pkg_private int		 	panel_row_to_y();
Pkg_private void		panel_sel_init();
Pkg_private Xv_opaque 		panel_set_avlist();
Pkg_private void		panel_set_bold_label_font();
Pkg_private Panel_item		panel_set_kbd_focus();
Pkg_private char               *panel_strsave();
Pkg_private Item_info 	       *panel_successor();
Pkg_private void		panel_text_caret_on();
Pkg_private void		panel_text_paint_label();
Pkg_private void		panel_unlink();
Pkg_private void		panel_update_extent();
Pkg_private void		panel_update_scrollbars();
Pkg_private void		panel_user_error();
Pkg_private int			panel_viewable_height();
Pkg_private Rect	       *panel_viewable_rect();
Pkg_private int			panel_viewable_width();
Pkg_private int			panel_wants_focus();
Pkg_private int			panel_width();
Pkg_private void		panel_yield_kbd_focus();
Pkg_private void                panel_autoscroll_start_itimer();
Pkg_private void                panel_autoscroll_stop_itimer();
Pkg_private void		panel_paint_border();
#ifdef OW_I18N
Xv_private  void		ml_panel_display_interm();
Pkg_private void 		ml_panel_saved_caret();
Pkg_private void 		panel_implicit_commit();
Pkg_private wchar_t	       *panel_strsave_wc();
Xv_private void	        	panel_preedit_display();
Xv_private void	        	panel_text_start();
Xv_private void	        	panel_text_draw();
Xv_private void	        	panel_text_done();
#endif /* OW_I18N */

#endif
