/* edit.h - main include file

   Copyright (C) 1996, 1997 the Free Software Foundation

   Authors: 1996, 1997 Paul Sheer

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef __EDIT_H
#define __EDIT_H

#ifdef MIDNIGHT

#ifdef HAVE_SLANG
#define HAVE_SYNTAXH 1
#endif

#    include <stdio.h>
#    include <stdarg.h>
#    include <sys/types.h>
#    ifdef HAVE_UNISTD_H
#    	 include <unistd.h>
#    endif
#    include <string.h>
#    include "src/tty.h"
#    include <sys/stat.h>
#    include <errno.h>

#    include <fcntl.h>

#    include <stdlib.h>
#    include <malloc.h>

#else       /* ! MIDNIGHT */

#    include "global.h"
#    include <stdio.h>
#    include <stdarg.h>
#    include <sys/types.h>
     
#    	 ifdef HAVE_UNISTD_H
#    	     include <unistd.h>
#    	 endif
     
#ifdef GTK
#    include <string.h>
#else
#    include <my_string.h>
#endif
#    include <sys/stat.h>
     
#    ifdef HAVE_FCNTL_H
#    	 include <fcntl.h>
#    endif
     
#    include <stdlib.h>
#    include <stdarg.h>

#    if TIME_WITH_SYS_TIME
#    	 include <sys/time.h>
#    	 include <time.h>
#    else
#    	 if HAVE_SYS_TIME_H
#    	     include <sys/time.h>
#    	 else
#    	     include <time.h>
#    	 endif
#    endif
 
#    include "regex.h"

#endif

#ifndef MIDNIGHT

#    include <signal.h>
#    include <X11/Xlib.h>
#    include <X11/Xutil.h>
#    include <X11/Xresource.h>
#    include "lkeysym.h"
#ifndef GTK
#    include "coolwidget.h"
#    include "app_glob.c"
#    include "coollocal.h"
#    include "stringtools.h"
#else
#    include "gtk/gtk.h"
#    include "gdk/gdkprivate.h"
#    include "gdk/gdk.h"
#    include "gtkedit.h"
#    include "editcmddef.h"
#    ifndef _
#        define _(x) x
#        define N_(x) x
#    endif
#endif

#else

#    include "src/main.h"		/* for char *shell */
#    include "src/mad.h"
#    include "src/dlg.h"
#    include "src/widget.h"
#    include "src/color.h"
#    include "src/dialog.h"
#    include "src/mouse.h"
#    include "src/global.h"
#    include "src/help.h"
#    include "src/key.h"
#    include "src/wtools.h"		/* for QuickWidgets */
#    include "src/win.h"
#    include "vfs/vfs.h"
#    include "src/menu.h"
#    include "src/regex.h"
#    define WANT_WIDGETS
     
#    define WIDGET_COMMAND (WIDGET_USER + 10)
#    define N_menus 5

#endif

#ifdef GTK
/* unistd.h defines _POSIX_VERSION on POSIX.1 systems. */
#if defined(HAVE_DIRENT_H) || defined(_POSIX_VERSION)
#   include <dirent.h>
#   define NLENGTH(dirent) (strlen ((dirent)->d_name))
#else
#   define dirent direct
#   define NLENGTH(dirent) ((dirent)->d_namlen)

#   ifdef HAVE_SYS_NDIR_H
#       include <sys/ndir.h>
#   endif /* HAVE_SYS_NDIR_H */

#   ifdef HAVE_SYS_DIR_H
#       include <sys/dir.h>
#   endif /* HAVE_SYS_DIR_H */

#   ifdef HAVE_NDIR_H
#       include <ndir.h>
#   endif /* HAVE_NDIR_H */
#endif /* not (HAVE_DIRENT_H or _POSIX_VERSION) */
#   ifndef _
#      define _(x) x
#      define N_(x) x
#   endif
#include "vfs/vfs.h"
#    define CDisplay gdk_display
#    define CRoot gdk_root_parent
#    define Window GtkEdit *
#endif

#define SEARCH_DIALOG_OPTION_NO_SCANF	1
#define SEARCH_DIALOG_OPTION_NO_REGEX	2
#define SEARCH_DIALOG_OPTION_NO_CASE	4
#define SEARCH_DIALOG_OPTION_BACKWARDS	8

#define SYNTAX_FILE "/.cedit/syntax"
#define CLIP_FILE "/.cedit/cooledit.clip"
#define MACRO_FILE "/.cedit/cooledit.macros"
#define BLOCK_FILE "/.cedit/cooledit.block"
#define ERROR_FILE "/.cedit/cooledit.error"
#define TEMP_FILE "/.cedit/cooledit.temp"
#define SCRIPT_FILE "/.cedit/cooledit.script"
#define EDIT_DIR "/.cedit"

#define EDIT_KEY_EMULATION_NORMAL 0
#define EDIT_KEY_EMULATION_EMACS  1

#define REDRAW_LINE          (1 << 0)
#define REDRAW_LINE_ABOVE    (1 << 1)
#define REDRAW_LINE_BELOW    (1 << 2)
#define REDRAW_AFTER_CURSOR  (1 << 3)
#define REDRAW_BEFORE_CURSOR (1 << 4)
#define REDRAW_PAGE          (1 << 5)
#define REDRAW_IN_BOUNDS     (1 << 6)
#define REDRAW_CHAR_ONLY     (1 << 7)
#define REDRAW_COMPLETELY    (1 << 8)

#define MOD_ABNORMAL		(1 << 0)
#define MOD_UNDERLINED		(1 << 1)
#define MOD_BOLD		(1 << 2)
#define MOD_HIGHLIGHTED		(1 << 3)
#define MOD_MARKED		(1 << 4)
#define MOD_ITALIC		(1 << 5)
#define MOD_CURSOR		(1 << 6)
#define MOD_INVERSE		(1 << 7)

#ifndef MIDNIGHT
#    ifdef GTK
#        define EDIT_TEXT_HORIZONTAL_OFFSET 0
#        define EDIT_TEXT_VERTICAL_OFFSET 0
#    else
#        define EDIT_TEXT_HORIZONTAL_OFFSET 4
#        define EDIT_TEXT_VERTICAL_OFFSET 3
#    endif
#else
#    define EDIT_TEXT_HORIZONTAL_OFFSET 0
#    define EDIT_TEXT_VERTICAL_OFFSET 1
#    define FONT_OFFSET_X 0
#    define FONT_OFFSET_Y 0
#endif

#define EDIT_RIGHT_EXTREME option_edit_right_extreme
#define EDIT_LEFT_EXTREME option_edit_left_extreme
#define EDIT_TOP_EXTREME option_edit_top_extreme
#define EDIT_BOTTOM_EXTREME option_edit_bottom_extreme

#define MAX_MACRO_LENGTH 1024

/*there are a maximum of ... */
#define MAXBUFF 1024
/*... edit buffers, each of which is ... */
#define EDIT_BUF_SIZE 0x10000
/* ...bytes in size. */

/*x / EDIT_BUF_SIZE equals x >> ... */
#define S_EDIT_BUF_SIZE 16

/* x % EDIT_BUF_SIZE is equal to x && ... */
#define M_EDIT_BUF_SIZE 0xFFFF

#define SIZE_LIMIT (EDIT_BUF_SIZE * (MAXBUFF - 2))
/* Note a 16k stack is 64k of data and enough to hold (usually) around 10
   pages of undo info. */

/* undo stack */
#define START_STACK_SIZE 32


/*some codes that may be pushed onto or returned from the undo stack: */
#define CURS_LEFT 601
#define CURS_RIGHT 602
#define DELETE 603
#define BACKSPACE 604
#define STACK_BOTTOM 605
#define CURS_LEFT_LOTS 606
#define CURS_RIGHT_LOTS 607
#define COLUMN_ON 608
#define COLUMN_OFF 609
#define MARK_1 1000
#define MARK_2 700000000
#define KEY_PRESS 1400000000

/*Tabs spaces: (sofar only HALF_TAB_SIZE is used: */
#define TAB_SIZE		option_tab_spacing
#define HALF_TAB_SIZE		((int) option_tab_spacing / 2)

struct macro {
    short command;
    short ch;
};

struct selection {
   unsigned char * text;
   int len;
};


#define RULE_CONTEXT		0x00FFF000UL
#define RULE_CONTEXT_SHIFT	12
#define RULE_WORD		0x00000FFFUL
#define RULE_WORD_SHIFT		0
#define RULE_ON_LEFT_BORDER	0x02000000UL
#define RULE_ON_RIGHT_BORDER	0x01000000UL

struct key_word {
    char *keyword;
    char first;
    char last;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
#define NO_COLOR ((unsigned long) -1);
    int line_start;
    int bg;
    int fg;
};

struct context_rule {
    int rule_number;
    char *left;
    char first_left;
    char last_left;
    char line_start_left;
    char *right;
    char first_right;
    char last_right;
    char line_start_right;
    int single_char;
    int between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    char *keyword_last_chars;
/* first word is word[1] */
    struct key_word **keyword;
};

struct _syntax_marker {
    long offset;
    unsigned long rule;
    struct _syntax_marker *next;
};

struct editor_widget {
#ifdef MIDNIGHT
    Widget widget;
#elif defined(GTK)
    GtkEdit *widget;
#else
    struct cool_widget *widget;
#endif
#define from_here num_widget_lines
    int num_widget_lines;
    int num_widget_columns;

#ifdef MIDNIGHT
    int have_frame;
#else
    int stopped;
#endif

    char *filename;		/* Name of the file */
    char *dir;			/* current directory */

/* dynamic buffers and curser position for editor: */
    long curs1;			/*position of the cursor from the beginning of the file. */
    long curs2;			/*position from the end of the file */
    unsigned char *buffers1[MAXBUFF + 1];	/*all data up to curs1 */
    unsigned char *buffers2[MAXBUFF + 1];	/*all data from end of file down to curs2 */

/* search variables */
    long search_start;		/* First character to start searching from */
    int found_len;		/* Length of found string or 0 if none was found */
    long found_start;		/* the found word from a search - start position */

/* display information */
    long last_byte;		/* Last byte of file */
    long start_display;		/* First char displayed */
    long start_col;		/* First displayed column, negative */
    long max_column;		/* The maximum cursor position ever reached used to calc hori scroll bar */
    long curs_row;		/*row position of curser on the screen */
    long curs_col;		/*column position on screen */
    int force;			/* how much of the screen do we redraw? */
    unsigned char overwrite;
    unsigned char modified;	/*has the file been changed?: 1 if char inserted or
				   deleted at all since last load or save */
#if defined(MIDNIGHT) || defined(GTK)
    int delete_file;			/* has the file been created in edit_load_file? Delete
			           it at end of editing when it hasn't been modified 
				   or saved */
#endif				   
    unsigned char highlight;
    long prev_col;		/*recent column position of the curser - used when moving
				   up or down past lines that are shorter than the current line */
    long curs_line;		/*line number of the cursor. */
    long start_line;		/*line nummber of the top of the page */

/* file info */
    long total_lines;		/*total lines in the file */
    long mark1;			/*position of highlight start */
    long mark2;			/*position of highlight end */
    int column1;			/*position of column highlight start */
    int column2;			/*position of column highlight end */
    long bracket;		/*position of a matching bracket */

/* cache speedup for line lookups */
#define N_LINE_CACHES	32
    int caches_valid;
    int line_numbers[N_LINE_CACHES];
    long line_offsets[N_LINE_CACHES];

/* undo stack and pointers */
    unsigned long stack_pointer;
    long *undo_stack;
    unsigned long stack_size;
    unsigned long stack_size_mask;
    unsigned long stack_bottom;
    struct stat stat;

/* syntax higlighting */
    struct _syntax_marker *syntax_marker;
    struct context_rule **rules;
    long last_get_rule;
    unsigned long rule;
    char *syntax_type;		/* description of syntax highlighting type being used */
    int explicit_syntax;	/* have we forced the syntax hi. type in spite of the filename? */

    int to_here;		/* dummy marker */


/* macro stuff */
    int macro_i;		/* -1 if not recording index to macro[] otherwise */
    struct macro macro[MAX_MACRO_LENGTH];
};

typedef struct editor_widget WEdit;

#ifndef MIDNIGHT

void edit_render_expose (WEdit * edit, XExposeEvent * xexpose);
#ifndef GTK
void edit_render_tidbits (struct cool_widget *w);
int eh_editor (CWidget * w, XEvent * xevent, CEvent * cwevent);
#endif
void edit_draw_menus (Window parent, int x, int y);
void edit_run_make (void);
void edit_change_directory (void);
int edit_man_page_cmd (WEdit * edit);
void edit_search_replace_dialog (Window parent, int x, int y, char **search_text, char **replace_text, char **arg_order, char *heading, int option);
void edit_search_dialog (WEdit * edit, char **search_text);
long edit_find (long search_start, unsigned char *expr, int *len, long last_byte, int (*get_byte) (void *, long), void *data);
void edit_set_foreground_colors (unsigned long normal, unsigned long bold, unsigned long italic);
void edit_set_background_colors (unsigned long normal, unsigned long abnormal, unsigned long marked, unsigned long marked_abnormal, unsigned long highlighted);
void edit_set_cursor_color (unsigned long c);
void draw_options_dialog (Window parent, int x, int y);
void CRefreshEditor (WEdit * edit);
void edit_set_user_command (void (*func) (WEdit *, int));
void edit_draw_this_line_proportional (WEdit * edit, long b, int curs_row, int start_column, int end_column);
unsigned char get_international_character (unsigned char key_press);
void edit_set_user_key_function (int (*user_def_key_func) (unsigned int, unsigned int, KeySym keysym));

#else

int edit_drop_hotkey_menu (WEdit * e, int key);
void edit_menu_cmd (WEdit * e);
void edit_init_menu_emacs (void);
void edit_init_menu_normal (void);
void edit_done_menu (void);
int edit_raw_key_query (char *heading, char *query, int cancel);
char *strcasechr (const unsigned char *s, int c);
int edit (const char *_file, int line);
int edit_translate_key (WEdit * edit, unsigned int x_keycode, long x_key, int x_state, int *cmd, int *ch);

#endif

#ifndef NO_INLINE_GETBYTE
int edit_get_byte (WEdit * edit, long byte_index);
#else
static inline int edit_get_byte (WEdit * edit, long byte_index)
{
    unsigned long p;
    if (byte_index >= (edit->curs1 + edit->curs2) || byte_index < 0)
	return '\n';

    if (byte_index >= edit->curs1) {
	p = edit->curs1 + edit->curs2 - byte_index - 1;
	return edit->buffers2[p >> S_EDIT_BUF_SIZE][EDIT_BUF_SIZE - (p & M_EDIT_BUF_SIZE) - 1];
    } else {
	return edit->buffers1[byte_index >> S_EDIT_BUF_SIZE][byte_index & M_EDIT_BUF_SIZE];
    }
}
#endif

char *edit_get_buffer_as_text (WEdit * edit);
int edit_load_file (WEdit * edit, const char *filename, const char *text, unsigned long text_size);
int edit_count_lines (WEdit * edit, long current, int upto);
long edit_move_forward (WEdit * edit, long current, int lines, long upto);
long edit_move_forward3 (WEdit * edit, long current, int cols, long upto);
long edit_move_backward (WEdit * edit, long current, int lines);
void edit_scroll_screen_over_cursor (WEdit * edit);
void edit_render_keypress (WEdit * edit);
void edit_scroll_upward (WEdit * edit, unsigned long i);
void edit_scroll_downward (WEdit * edit, int i);
void edit_scroll_right (WEdit * edit, int i);
void edit_scroll_left (WEdit * edit, int i);
int edit_get_col (WEdit * edit);
long edit_bol (WEdit * edit, long current);
long edit_eol (WEdit * edit, long current);
void edit_update_curs_row (WEdit * edit);
void edit_update_curs_col (WEdit * edit);

void edit_block_copy_cmd (WEdit * edit);
void edit_block_move_cmd (WEdit * edit);
int edit_block_delete_cmd (WEdit * edit);
int edit_block_delete (WEdit * edit);
void edit_delete_line (WEdit * edit);

int edit_delete (WEdit * edit);
void edit_insert (WEdit * edit, int c);
int edit_cursor_move (WEdit * edit, long increment);
void edit_push_action (WEdit * edit, long c,...);
void edit_push_key_press (WEdit * edit);
void edit_insert_ahead (WEdit * edit, int c);
int edit_save_file (WEdit * edit, const char *filename);
int edit_save_cmd (WEdit * edit);
int edit_save_confirm_cmd (WEdit * edit);
int edit_save_as_cmd (WEdit * edit);
WEdit *edit_init (WEdit * edit, int lines, int columns, const char *filename, const char *text, const char *dir, unsigned long text_size);
int edit_clean (WEdit * edit);
int edit_renew (WEdit * edit);
int edit_new_cmd (WEdit * edit);
int edit_reload (WEdit * edit, const char *filename, const char *text, const char *dir, unsigned long text_size);
int edit_load_cmd (WEdit * edit);
void edit_mark_cmd (WEdit * edit, int unmark);
void edit_set_markers (WEdit * edit, long m1, long m2, int c1, int c2);
void edit_push_markers (WEdit * edit);
void edit_quit_cmd (WEdit * edit);
void edit_replace_cmd (WEdit * edit, int again);
void edit_search_cmd (WEdit * edit, int again);
int edit_save_block_cmd (WEdit * edit);
int edit_insert_file_cmd (WEdit * edit);
int edit_insert_file (WEdit * edit, const char *filename);
void edit_block_process_cmd (WEdit * edit, const char *shell_cmd, int block);
char *catstrs (const char *first,...);
void edit_refresh_cmd (WEdit * edit);
void edit_date_cmd (WEdit * edit);
void edit_goto_cmd (WEdit * edit);
int eval_marks (WEdit * edit, long *start_mark, long *end_mark);
void edit_status (WEdit * edit);
int edit_execute_command (WEdit * edit, int command, int char_for_insertion);
int edit_execute_key_command (WEdit * edit, int command, int char_for_insertion);
void edit_update_screen (WEdit * edit);
int edit_printf (WEdit * e, const char *fmt,...);
int edit_print_string (WEdit * e, const char *s);
void edit_move_to_line (WEdit * e, long line);
void edit_move_display (WEdit * e, long line);
void edit_word_wrap (WEdit * edit);
unsigned char *edit_get_block (WEdit * edit, long start, long finish, int *l);
int edit_sort_cmd (WEdit * edit);
void edit_help_cmd (WEdit * edit);
void edit_left_word_move (WEdit * edit);
void edit_right_word_move (WEdit * edit);
void edit_get_selection (WEdit * edit);

int edit_save_macro_cmd (WEdit * edit, struct macro macro[], int n);
int edit_load_macro_cmd (WEdit * edit, struct macro macro[], int *n, int k);
void edit_delete_macro_cmd (WEdit * edit);

int edit_copy_to_X_buf_cmd (WEdit * edit);
int edit_cut_to_X_buf_cmd (WEdit * edit);
void edit_paste_from_X_buf_cmd (WEdit * edit);

void edit_paste_from_history (WEdit *edit);

void edit_split_filename (WEdit * edit, char *name);

#ifdef MIDNIGHT
#define CWidget Widget
#elif defined(GTK)
#define CWidget GtkEdit
#endif
void edit_set_syntax_change_callback (void (*callback) (CWidget *));
void edit_load_syntax (WEdit * edit, char **names, char *type);
void edit_free_syntax_rules (WEdit * edit);
void edit_get_syntax_color (WEdit * edit, long byte_index, int *fg, int *bg);


#ifdef MIDNIGHT

/* put OS2/NT/WIN95 defines here */

#    ifdef OS2_NT
#    	 define MY_O_TEXT O_TEXT
#    else
#    	 define MY_O_TEXT 0
#    endif

#    define FONT_PIX_PER_LINE 1
#    define FONT_MEAN_WIDTH 1
     
#    define get_sys_error(s) (s)
#    define open mc_open
#    define close(f) mc_close(f)
#    define read(f,b,c) mc_read(f,b,c)
#    define write(f,b,c) mc_write(f,b,c)
#    define stat(f,s) mc_stat(f,s)
#    define mkdir(s,m) mc_mkdir(s,m)
#    define itoa MY_itoa

#    define edit_get_load_file(d,f,h) input_dialog (h, " Enter file name: ", f)
#    define edit_get_save_file(d,f,h) input_dialog (h, " Enter file name: ", f)
#    define CMalloc(x) malloc(x)
     
#    define set_error_msg(s) edit_init_error_msg = strdup(s)

#    ifdef _EDIT_C

#         define edit_error_dialog(h,s) set_error_msg(s)
char *edit_init_error_msg = NULL;

#    else				/* ! _EDIT_C */

#    define edit_error_dialog(h,s) query_dialog (h, s, 0, 1, _("&Dismiss"))
#    define edit_message_dialog(h,s) query_dialog (h, s, 0, 1, _("&Ok"))
extern char *edit_init_error_msg;

#    endif				/* ! _EDIT_C */


#    define get_error_msg(s) edit_init_error_msg
#    define edit_query_dialog2(h,t,a,b) query_dialog(h,t,0,2,a,b)
#    define edit_query_dialog3(h,t,a,b,c) query_dialog(h,t,0,3,a,b,c)
#    define edit_query_dialog4(h,t,a,b,c,d) query_dialog(h,t,0,4,a,b,c,d)

#else				/* ! MIDNIGHT */

#    ifdef GTK
#        define get_sys_error(s) (s)

#        define open mc_open
#        define close(f) mc_close(f)
#        define read(f,b,c) mc_read(f,b,c)
#        define write(f,b,c) mc_write(f,b,c)
#        define stat(f,s) mc_stat(f,s)
#        define mkdir(s,m) mc_mkdir(s,m)

#        define itoa MY_itoa
#        define CMalloc(x) malloc(x)

#        define EDITOR_NO_FILE			(1<<3)
#        define EDITOR_NO_SCROLL		(1<<4)
#        define EDITOR_NO_TEXT			(1<<5)
#        define EDITOR_HORIZ_SCROLL		(1<<6)

#include <gdk/gdkprivate.h>
#        define CWindowOf(w) (w)
#        define CHeightOf(w) ((w)->editable.widget.allocation.height)
#        define CWidthOf(w) ((w)->editable.widget.allocation.width)
#        define COptionsOf(w) ((w)->options)

#        define cache_type unsigned int

/* font dimensions */
#        define FONT_OVERHEAD		gtk_edit_option_text_line_spacing
#        define FONT_BASE_LINE		(FONT_OVERHEAD + gtk_edit_option_font_ascent)
#        define FONT_HEIGHT		(gtk_edit_option_font_ascent + gtk_edit_option_font_descent)
#        define FONT_PIX_PER_LINE	(FONT_OVERHEAD + FONT_HEIGHT)
#        define FONT_MEAN_WIDTH	gtk_edit_option_font_mean_width

#        define FONT_OFFSET_X 0
#        define FONT_OFFSET_Y		FONT_BASE_LINE

#        define per_char gtk_edit_font_width_per_char

#        ifndef _GTK_EDIT_C
extern guchar gtk_edit_font_width_per_char[256];
extern int gtk_edit_option_text_line_spacing;
extern int gtk_edit_option_font_ascent;
extern int gtk_edit_option_font_descent;
extern int gtk_edit_option_font_mean_width;
extern int gtk_edit_fixed_font;
#        endif

/* start temporary */

#        define COLOR_BLACK 0
#        define COLOR_WHITE 1
#        define CURSOR_TYPE_EDITOR 0

#        define WIN_MESSAGES GTK_WINDOW_TOPLEVEL, 20, 20
#        define option_text_line_spacing 1
#        define fixed_font 0

#define color_palette(x) win->color[x].pixel

#define DndNotDnd	-1
#define DndUnknown	0
#define DndRawData	1
#define DndFile		2
#define DndFiles	3
#define DndText		4
#define DndDir		5
#define DndLink		6
#define DndExe		7
#define DndURL		8
#define DndMIME         9

#define DndEND		10

#define dnd_null_term_type(d) \
	((d) == DndFile || (d) == DndText || (d) == DndDir || \
	(d) == DndLink || (d) == DndExe || (d) == DndURL)



/* end temporary */

#    else

#        define WIN_MESSAGES edit->widget ? edit->widget->mainid : CRoot, 20, 20

#    endif

#    define MY_O_TEXT 0

#    ifdef GTK

#        ifndef min
#            define min(x,y)     (((x) < (y)) ? (x) : (y))
#        endif

#        ifndef max
#            define max(x,y)     (((x) > (y)) ? (x) : (y))
#        endif

/*
extern Display             *gdk_display;
extern Window               gdk_root_window;
*/

enum {
 match_file, match_normal
};

#        define edit_get_load_file(d,f,h) gtk_edit_dialog_get_load_file(d,f,h)
#        define edit_get_save_file(d,f,h) gtk_edit_dialog_get_save_file(d,f,h)
#        define edit_error_dialog(h,t) gtk_edit_dialog_error(h,"%s",t)
#        define edit_message_dialog(h,t) gtk_edit_dialog_message(0,h,"%s",t)
#        define edit_query_dialog2(h,t,a,b) gtk_edit_dialog_query(h,t,a,b,0)
#        define edit_query_dialog3(h,t,a,b,c) gtk_edit_dialog_query(h,t,a,b,c,0)
#        define edit_query_dialog4(h,t,a,b,c,d) gtk_edit_dialog_query(h,t,a,b,c,d,0)

#        define CError(x) printf("Error: %s\n",x)
#        define CIsDropAcknowledge(a,b) DndNotDnd
#        define CGetDrop(e,d,s,x,y) DndNotDnd
#        define CDropAcknowledge(x) 
/* #        define edit_get_syntax_color(e,i,f,b)  */
#        define get_international_character(k) 0
#        define compose_key_pressed 0

#    else

#        define edit_get_load_file(d,f,h) CGetLoadFile(WIN_MESSAGES,d,f,h)
#        define edit_get_save_file(d,f,h) CGetSaveFile(WIN_MESSAGES,d,f,h)
#        define edit_error_dialog(h,t) CErrorDialog(WIN_MESSAGES,h,"%s",t)
#        define edit_message_dialog(h,t) CMessageDialog(WIN_MESSAGES,0,h,"%s",t)
#        define edit_query_dialog2(h,t,a,b) CQueryDialog(WIN_MESSAGES,h,t,a,b,0)
#        define edit_query_dialog3(h,t,a,b,c) CQueryDialog(WIN_MESSAGES,h,t,a,b,c,0)
#        define edit_query_dialog4(h,t,a,b,c,d) CQueryDialog(WIN_MESSAGES,h,t,a,b,c,d,0)
#    endif

#endif				/* ! MIDNIGHT */

extern char *home_dir;

#define NUM_SELECTION_HISTORY 32

#ifndef MAX_PATH_LEN
#ifdef PATH_MAX
#define MAX_PATH_LEN PATH_MAX
#else
#define MAX_PATH_LEN 1024
#endif
#endif

#ifdef _EDIT_C

struct selection selection =
{0, 0};
int current_selection = 0;
/* Note: selection.text = selection_history[current_selection].text */
struct selection selection_history[NUM_SELECTION_HISTORY] =
{
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
};

#ifdef MIDNIGHT
/*
   what editor are we going to emulate? one of EDIT_KEY_EMULATION_NORMAL
   or EDIT_KEY_EMULATION_EMACS
 */
int edit_key_emulation = EDIT_KEY_EMULATION_NORMAL;
#endif	/* ! MIDNIGHT */

int option_word_wrap_line_length = 72;
int option_typewriter_wrap = 0;
int option_auto_para_formatting = 0;
int option_international_characters = 0;
int option_tab_spacing = 8;
int option_fill_tabs_with_spaces = 0;
int option_return_does_auto_indent = 1;
int option_backspace_through_tabs = 0;
int option_fake_half_tabs = 1;
int option_save_mode = 0;
int option_backup_ext_int = -1;
int option_find_bracket = 1;
int option_max_undo = 32768;

int option_editor_fg_normal = 26;
int option_editor_fg_bold = 8;
int option_editor_fg_italic = 10;

int option_edit_right_extreme = 0;
int option_edit_left_extreme = 0;
int option_edit_top_extreme = 0;
int option_edit_bottom_extreme = 0;

int option_editor_bg_normal = 1;
int option_editor_bg_abnormal = 0;
int option_editor_bg_marked = 2;
int option_editor_bg_marked_abnormal = 9;
int option_editor_bg_highlighted = 12;
int option_editor_fg_cursor = 18;

char *option_whole_chars_search = "0123456789abcdefghijklmnopqrstuvwxyz_";
char *option_whole_chars_move = "0123456789abcdefghijklmnopqrstuvwxyz_; ,[](){}";
char *option_backup_ext = "~";

#else				/* ! _EDIT_C */

extern struct selection selection;
extern struct selection selection_history[];
extern int current_selection;

#ifdef MIDNIGHT
/*
   what editor are we going to emulate? one of EDIT_KEY_EMULATION_NORMAL
   or EDIT_KEY_EMULATION_EMACS
 */
extern int edit_key_emulation;
#endif	/* ! MIDNIGHT */

extern int option_word_wrap_line_length;
extern int option_typewriter_wrap;
extern int option_auto_para_formatting;
extern int option_international_characters;
extern int option_tab_spacing;
extern int option_fill_tabs_with_spaces;
extern int option_return_does_auto_indent;
extern int option_backspace_through_tabs;
extern int option_fake_half_tabs;
extern int option_save_mode;
extern int option_backup_ext_int;
extern int option_find_bracket;
extern int option_max_undo;

extern int option_editor_fg_normal;
extern int option_editor_fg_bold;
extern int option_editor_fg_italic;

extern int option_edit_right_extreme;
extern int option_edit_left_extreme;
extern int option_edit_top_extreme;
extern int option_edit_bottom_extreme;

extern int option_editor_bg_normal;
extern int option_editor_bg_abnormal;
extern int option_editor_bg_marked;
extern int option_editor_bg_marked_abnormal;
extern int option_editor_bg_highlighted;
extern int option_editor_fg_cursor;

extern char *option_whole_chars_search;
extern char *option_whole_chars_move;
extern char *option_backup_ext;

extern int edit_confirm_save;

#endif				/* ! _EDIT_C */
#endif 				/* __EDIT_H */
