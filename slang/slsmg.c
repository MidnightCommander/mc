/* SLang Screen management routines */
#include "slinclud.h"

#include <stdio.h>
#include <string.h>
#include "slang.h"
#include "_slang.h"

typedef struct
  {
     int n;                    /* number of chars written last time */
     int flags;                /* line untouched, etc... */
     SLsmg_Char_Type *old, *neew;
#ifndef IBMPC_SYSTEM
     unsigned long old_hash, new_hash;
#endif
  }
Screen_Type;

#define TOUCHED 0x1
#define TRASHED 0x2
static int Screen_Trashed;

Screen_Type SL_Screen[SLTT_MAX_SCREEN_ROWS];
static int Start_Col, Start_Row;
static unsigned int Screen_Cols, Screen_Rows;
static int This_Row, This_Col;
static SLsmg_Color_Type This_Color;

static int UTF8_Mode = -1;

#if SLTT_HAS_NON_BCE_SUPPORT && !defined(IBMPC_SYSTEM)
#define REQUIRES_NON_BCE_SUPPORT 1
static int Bce_Color_Offset;
#endif

int SLsmg_Newline_Behavior = SLSMG_NEWLINE_IGNORED;
int SLsmg_Backspace_Moves = 0;

#if SLSMG_HAS_EMBEDDED_ESCAPE
/* If non-zero, interpret escape sequences ESC [ x m as an embedded set color
 * sequence.  The 'x' is a decimal integer that specifies the color.  The sequence
 * ends at m.  Examples:  \e[3m --> set color 3.  \e[272m --> color=272.
 * Note: These escape sequences are NOT ANSI, though similar.  ANSI permits
 * sequences such as \e[32;44m to set the foreground to green/blue.  This interface
 * will support such sequences, _but_ in will map such squences to the sum of 
 * the colors: \e[32;44m --> color (32+44)=76.
 * 
 * In addition to 'm', ']' is also supported.
 */
static int Embedded_Escape_Mode = 0;
int SLsmg_embedded_escape_mode (int mode)
{
   int old_mode = Embedded_Escape_Mode;
   Embedded_Escape_Mode = mode;
   return old_mode;
}

/* This function gets called with u pointing at what may be '['.  If it sucessfully
 * parses the escape sequence, *up and the color will be updated.
 */
static int parse_embedded_escape (SLuchar_Type *u, SLuchar_Type *umax, 
				  SLsmg_Color_Type default_color, 
				  SLuchar_Type **up, SLsmg_Color_Type *colorp)
{
   unsigned int val;
   SLuchar_Type ch;

   if ((u < umax) && (*u != '['))
     return -1;

   u++;
   if ((u < umax) && ((*u == 'm') || (*u == ']')))
     {
	*colorp = default_color;       /* ESC[m */
	*up = u+1;
	return 0;
     }

   val = 0;
   while ((u < umax)
	  && (((ch = *u) >= '0') && (ch <= '9')))
     {
	val = 10*val + (ch - '0');
	u++;
     }
   if ((u < umax) && ((*u == 'm') || (*u == ']')) && (val <= SLSMG_MAX_COLORS))
     {
# ifdef REQUIRES_NON_BCE_SUPPORT
	val += Bce_Color_Offset;
#endif
	*colorp = (SLsmg_Color_Type) val;
	*up = u + 1;
	return 0;
     }
   return -1;
}

static void parse_embedded_set_color (SLuchar_Type *u, SLuchar_Type *umax,
				      SLsmg_Color_Type default_color)
{
   SLsmg_Color_Type color = default_color;

   while (u < umax)
     {
	if (*u++ == 033)
	  (void) parse_embedded_escape (u, umax, default_color, &u, &color);
     }
   if (color == default_color)
     return;
   
#ifdef REQUIRES_NON_BCE_SUPPORT
   color -= Bce_Color_Offset;
#endif
   SLsmg_set_color (color);
}
#endif

/* Backward compatibility. Not used. */
/* int SLsmg_Newline_Moves; */
static int *tt_Screen_Rows = NULL;
static int *tt_Screen_Cols = NULL;
static int *tt_unicode_ok;
   
static void (*tt_normal_video)(void);
static void (*tt_goto_rc)(int, int);
static void (*tt_cls) (void);
static void (*tt_del_eol) (void);
static void (*tt_smart_puts) (SLsmg_Char_Type *, SLsmg_Char_Type *, int, int);
static int (*tt_flush_output) (void);
static int (*tt_reset_video) (void);
static int (*tt_init_video) (void);

#ifndef IBMPC_SYSTEM
static void (*tt_set_scroll_region)(int, int);
static void (*tt_reverse_index)(int);
static void (*tt_reset_scroll_region)(void);
static void (*tt_delete_nlines)(int);
#endif

#ifndef IBMPC_SYSTEM
static int *tt_Term_Cannot_Scroll;
static int *tt_Has_Alt_Charset;
static char **tt_Graphics_Char_Pairs;
#else
static int *tt_Has_Alt_Charset = NULL;
static char **tt_Graphics_Char_Pairs = NULL;
#endif

static int Smg_Inited;

/* This is necessary because the windoze run-time linker cannot perform 
 * relocations on its own.
 */
static void init_tt_symbols (void)
{
   tt_Screen_Rows = &SLtt_Screen_Rows;
   tt_Screen_Cols = &SLtt_Screen_Cols;
   tt_unicode_ok = &_pSLtt_UTF8_Mode;
   
   tt_normal_video = SLtt_normal_video;
   tt_goto_rc = SLtt_goto_rc;
   tt_cls = SLtt_cls;
   tt_del_eol = SLtt_del_eol;
   tt_smart_puts = SLtt_smart_puts;
   tt_flush_output = SLtt_flush_output;
   tt_reset_video = SLtt_reset_video;
   tt_init_video = SLtt_init_video;

#ifndef IBMPC_SYSTEM
   tt_set_scroll_region = SLtt_set_scroll_region;
   tt_reverse_index = SLtt_reverse_index;
   tt_reset_scroll_region = SLtt_reset_scroll_region;
   tt_delete_nlines = SLtt_delete_nlines;
#endif

#ifndef IBMPC_SYSTEM
   tt_Term_Cannot_Scroll = &SLtt_Term_Cannot_Scroll;
   tt_Has_Alt_Charset = &SLtt_Has_Alt_Charset;
   tt_Graphics_Char_Pairs = &SLtt_Graphics_Char_Pairs;
#else
   tt_Has_Alt_Charset = NULL;
   tt_Graphics_Char_Pairs = NULL;
#endif
}

/* Unicode box drawing characters */

/* This lookup table is indexed by the vt100 characters */
static SLwchar_Type ACS_Map[128];

typedef struct
{
   unsigned char vt100_char;
   unsigned char ascii;
   SLwchar_Type unicode;
}
ACS_Def_Type;

static SLCONST ACS_Def_Type UTF8_ACS_Map[] =
{
   {'+', '>', 0x2192 }, /* RIGHTWARDS ARROW */
   {',', '<', 0x2190 }, /* LEFTWARDS ARROW */
   {'-', '^', 0x2191 }, /* UPWARDS ARROW */
   {'.', 'v', 0x2193 }, /* DOWNWARDS ARROW */
   {'0', '#', 0x25AE }, /* BLACK VERTICAL RECTANGLE */
   {'`', '+', 0x25C6 }, /* BLACK DIAMOND */
   {'a', ':', 0x2592 }, /* MEDIUM SHADE */
   {'f', '\'', 0x00B0 },/* DEGREE SIGN */
   {'g', '#', 0x00B1 }, /* PLUS-MINUS SIGN */
   {'h', '#', 0x2592 }, /* MEDIUM SHADE */
   {'i', '#', 0x2603 }, /* SNOWMAN */
   {'j', '+', 0x2518 }, /* BOX DRAWINGS LIGHT UP AND LEFT */
   {'k', '+', 0x2510 }, /* BOX DRAWINGS LIGHT DOWN AND LEFT */
   {'l', '+', 0x250c }, /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
   {'m', '+', 0x2514 }, /* BOX DRAWINGS LIGHT UP AND RIGHT */
   {'n', '+', 0x253C }, /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
   {'o', '~', 0x23BA }, /* HORIZONTAL SCAN LINE-1 */
   {'p', '-', 0x23BB }, /* HORIZONTAL SCAN LINE-3 (ncurses addition) */
   {'q', '-', 0x2500 }, /* BOX DRAWINGS LIGHT HORIZONTAL */
   {'r', '-', 0x23BC }, /* HORIZONTAL SCAN LINE-7 (ncurses addition) */
   {'s', '_', 0x23BD }, /* HORIZONTAL SCAN LINE-9 */
   {'t', '+', 0x251C }, /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
   {'u', '+', 0x2524 }, /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
   {'v', '+', 0x2534 }, /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
   {'w', '+', 0x252C }, /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
   {'x', '|', 0x2502 }, /* BOX DRAWINGS LIGHT VERTICAL */
   {'y', '<', 0x2264 }, /* LESS-THAN OR EQUAL TO (ncurses addition) */
   {'z', '>', 0x2265 }, /* GREATER-THAN OR EQUAL TO (ncurses addition) */
   {'{', '*', 0x03C0 }, /* GREEK SMALL LETTER PI (ncurses addition) */
   {'|', '!', 0x2260 }, /* NOT EQUAL TO (ncurses addition) */
   {'}', 'f', 0x00A3 }, /* POUND SIGN (ncurses addition) */
   {'~', 'o', 0x00B7 }, /* MIDDLE DOT */
   {0, 0, 0}
};

#define ACS_MODE_NONE	       -1
#define ACS_MODE_AUTO		0
#define ACS_MODE_UNICODE	1
#define ACS_MODE_TERMINFO	2
#define ACS_MODE_ASCII		3

static int Current_ACS_Mode = ACS_MODE_NONE;
static void init_acs (int mode)
{
   unsigned int i;
   SLCONST ACS_Def_Type *acs;

   if (Current_ACS_Mode == mode)
     return;

   for (i = 0; i < 0x80; i++)
     ACS_Map[i] = ' ';

   if (mode == ACS_MODE_AUTO)
     {
	if (UTF8_Mode && 
	    (tt_unicode_ok != NULL) && (*tt_unicode_ok > 0))
	  mode = ACS_MODE_UNICODE;
	else
	  mode = ACS_MODE_TERMINFO;
     }

   switch (mode)
     {
      case ACS_MODE_UNICODE:
	SLsmg_Display_Eight_Bit = 0xA0;
	acs = UTF8_ACS_Map;
	while (acs->vt100_char != 0)
	  {
	     ACS_Map[acs->vt100_char] = acs->unicode;
	     acs++;
	  }
	break;
	
      case ACS_MODE_TERMINFO:
	if ((tt_Has_Alt_Charset != NULL)
	    && *tt_Has_Alt_Charset
	    && (tt_Graphics_Char_Pairs != NULL)
	    && (*tt_Graphics_Char_Pairs != NULL))
	  {
	     unsigned char *p = (unsigned char *) *tt_Graphics_Char_Pairs;
	     unsigned char *pmax = p + strlen ((char *) p);
	     
	     while (p < pmax)
	       {
		  unsigned char ch = *p++;
		  ACS_Map[ch & 0x7F] = *p++;
	       }
	     break;
	  }
	mode = ACS_MODE_ASCII;
	/* drop */
      case ACS_MODE_ASCII:
      default:
	acs = UTF8_ACS_Map;
	while (acs->vt100_char != 0)
	  {
	     ACS_Map[acs->vt100_char] = acs->ascii;
	     acs++;
	  }
	break;
     }

   Current_ACS_Mode = mode;
}

static void blank_line (SLsmg_Char_Type *c, unsigned int n, SLwchar_Type wch)
{
   SLsmg_Char_Type *cmax = c + n;
   SLsmg_Color_Type color = This_Color;

   memset ((char *)c, 0, n*sizeof(SLsmg_Char_Type));
   while (c < cmax)
     {
	c->nchars = 1;
	c->wchars[0] = wch;
	c->color = color;
	c++;
     }
}

static void clear_region (int row, int n, SLwchar_Type ch)
{
   int i;
   int imax = row + n;

   if (imax > (int) Screen_Rows) imax = (int) Screen_Rows;
   if (row < 0)
     row = 0;

   for (i = row; i < imax; i++)
     {						  
	blank_line (SL_Screen[i].neew, Screen_Cols, ch);
	SL_Screen[i].flags |= TOUCHED;
     }
}

void SLsmg_erase_eol (void)
{
   int r, c;

   if (Smg_Inited == 0) return;

   c = This_Col - Start_Col;
   r = This_Row - Start_Row;

   if ((r < 0) || (r >= (int)Screen_Rows)) return;
   if (c < 0) c = 0; else if (c >= (int)Screen_Cols) return;
   blank_line (SL_Screen[This_Row].neew + c , Screen_Cols - c, 0x20);
   SL_Screen[This_Row].flags |= TOUCHED;
}

static void scroll_up (void)
{
   unsigned int i, imax;
   SLsmg_Char_Type *neew;

   neew = SL_Screen[0].neew;
   imax = Screen_Rows - 1;
   for (i = 0; i < imax; i++)
     {
	SL_Screen[i].neew = SL_Screen[i + 1].neew;
	SL_Screen[i].flags |= TOUCHED;
     }
   SL_Screen[i].neew = neew;
   SL_Screen[i].flags |= TOUCHED;
   blank_line (neew, Screen_Cols, 0x20);
   This_Row--;
}

void SLsmg_gotorc (int r, int c)
{
   This_Row = r;
   This_Col = c;
}

int SLsmg_get_row (void)
{
   return This_Row;
}

int SLsmg_get_column (void)
{
   return This_Col;
}

void SLsmg_erase_eos (void)
{
   if (Smg_Inited == 0) return;

   SLsmg_erase_eol ();
   clear_region (This_Row + 1, (int)Screen_Rows, 0x20);
}

static int This_Alt_Char;

void SLsmg_set_char_set (int i)
{
#ifdef IBMPC_SYSTEM
   (void) i;
#else
   if (i != 0)
     This_Alt_Char = SLSMG_ACS_MASK;
   else This_Alt_Char = 0;

   This_Color &= SLSMG_COLOR_MASK;
   This_Color |= This_Alt_Char;
#endif
}

void SLsmg_set_color (SLsmg_Color_Type color)
{
#ifdef REQUIRES_NON_BCE_SUPPORT
   color += Bce_Color_Offset;
#endif
   This_Color = color | This_Alt_Char;
}

void SLsmg_reverse_video (void)
{
   SLsmg_set_color (1);
}

void SLsmg_normal_video (void)
{
   SLsmg_set_color (0);
}

static int point_visible (int col_too)
{
   return ((This_Row >= Start_Row) && (This_Row < Start_Row + (int)Screen_Rows)
	   && ((col_too == 0)
	       || ((This_Col >= Start_Col)
		   && (This_Col < Start_Col + (int)Screen_Cols))));
}

#define NEXT_CHAR_CELL \
   { \
      if (p < pmax) \
	{ \
	   if ((p->nchars != i) || (p->color != color)) flags |= TOUCHED; \
	   p->nchars = i; \
	   p->color = color; \
	   p++; \
	} \
      i = 0; \
      col++; \
   } (void) 0


#define ADD_TO_CHAR_CELL(wc) \
   { \
      if ((p < pmax) && (p->wchars[i] != wc)) \
	{ \
	   p->wchars[i] = wc; \
	   flags |= TOUCHED; \
	} \
      i++; \
   } (void) 0

#define ADD_CHAR_OR_BREAK(ch) \
   if (col >= start_col) \
       { \
	  if (i != 0) NEXT_CHAR_CELL; \
	  if (last_was_double_width) \
	    { \
	       last_was_double_width = 0; \
	       NEXT_CHAR_CELL; \
	    } \
	  if (col >= max_col) break; \
	  ADD_TO_CHAR_CELL(ch); \
       } \
   else col++


void SLsmg_write_chars (unsigned char *u, unsigned char *umax)
{
   SLsmg_Char_Type *p, *pmax;
   SLsmg_Color_Type color;
   int flags;
   int col, start_col, max_col;
   int newline_flag;
   int utf8_mode = UTF8_Mode;
   unsigned char display_8bit;
   int last_was_double_width = 0;
   int alt_char_set_flag;
   unsigned int i;
#if SLSMG_HAS_EMBEDDED_ESCAPE
   SLsmg_Color_Type default_color;
#endif
   if (Smg_Inited == 0) return;
   
   display_8bit = (unsigned char) SLsmg_Display_Eight_Bit;
   if (utf8_mode)
     display_8bit = 0xA0;

   color = This_Color;
   /* If we are using unicode characters for the line drawing characters, then
    * do not attempt to use the terminals alternate character set
    */
   alt_char_set_flag = (color & SLSMG_ACS_MASK);
   if (Current_ACS_Mode == ACS_MODE_UNICODE)
     color = color & ~SLSMG_ACS_MASK;
   
#if SLSMG_HAS_EMBEDDED_ESCAPE
   default_color = color;	       /* used for ESC[m */
#endif

   top:				       /* get here only on newline */

   newline_flag = 0;
   start_col = Start_Col;

   if (point_visible (0) == 0) return;

   col = This_Col;
   max_col = start_col + Screen_Cols;

   p = SL_Screen[This_Row - Start_Row].neew;
   pmax = p + Screen_Cols;

   if (col >= start_col)
     {
	p += (col - start_col);
	if ((p < pmax) && (p->nchars == 0))
	  {
	     /* It looks like we are about to overwrite the right side of a 
	      * double width character.
	      */
	     if (col > start_col)
	       {
		  p--;
		  p->nchars = 1;
		  p->wchars[0] = ' ';
		  p++;
	       }
	  }
     }
   
   
   flags = SL_Screen[This_Row - Start_Row].flags;
   i = 0;
   
   while (u < umax)
     {
	SLwchar_Type wc;
	unsigned int width, nconsumed;

     	if (*u < (SLuchar_Type) 0x80)		       /* ASCII */
	  {
	     unsigned char ch;

	     ch = (unsigned char) *u++;

	     if (alt_char_set_flag)
	       {
		  wc = ACS_Map[ch];
		  ADD_CHAR_OR_BREAK(wc);
		  continue;
	       }

	     if ((ch >= (SLuchar_Type)0x20) && (ch < (SLuchar_Type)0x7F))
	       {
		  ADD_CHAR_OR_BREAK(ch);
		  continue;
	       }
	     
	     if ((ch == '\t') && (SLsmg_Tab_Width > 0))
	       {
		  do
		    {
		       if (col < start_col)
			 col++;
		       else
			 {
			    ADD_CHAR_OR_BREAK(' ');
			    NEXT_CHAR_CELL;
			 }
		    }
		  while (col % SLsmg_Tab_Width);
		  continue;
	       }
	     
	     if ((ch == '\n')
		 && (SLsmg_Newline_Behavior != SLSMG_NEWLINE_PRINTABLE))
	       {
		  newline_flag = 1;
		  break;
	       }
	     
	     if ((ch == 0x8) && SLsmg_Backspace_Moves)
	       {
		  if (col != 0) 
		    {
		       if (i != 0) 
			 {
			    NEXT_CHAR_CELL;
			    col--;
			    p--;
			 }
		       col--;
		       p--;
		    }
		  continue;
	       }
#if SLSMG_HAS_EMBEDDED_ESCAPE
	     if ((ch == 033) && Embedded_Escape_Mode)
	       {
		  SLsmg_Color_Type next_color;

		  if (0 == parse_embedded_escape (u, umax, default_color, &u, &next_color))
		    {
		       if (i != 0)
			 NEXT_CHAR_CELL;
		       color = next_color;
		       continue;
		    }
	       }
#endif
	     ADD_CHAR_OR_BREAK('^');
	     if (ch == 127) ch = '?'; else ch = ch + '@';
	     ADD_CHAR_OR_BREAK (ch);
	     continue;
	  }

	nconsumed = 1;
	if ((utf8_mode == 0)
	    || (NULL == SLutf8_decode (u, umax, &wc, &nconsumed)))
	  {
	     unsigned int ii, jj;
	     unsigned char hexbuf[8];
	     
	     if ((utf8_mode == 0) 
		 && display_8bit && (*u >= display_8bit))
	       {
		  ADD_CHAR_OR_BREAK(*u);
	       }
	     else for (ii = 0; ii < nconsumed; ii++)
	       {
		  sprintf ((char *)hexbuf, "<%02X>", u[ii]);
		  for (jj = 0; jj < 4; jj++)
		    {
		       ADD_CHAR_OR_BREAK (hexbuf[jj]);
		    }
	       }
	     u += nconsumed;
	     continue;
	  }

	u += nconsumed;
	if (wc < (SLwchar_Type)display_8bit)
	  {
	     unsigned char hexbuf[8];
	     unsigned int jj;

	     sprintf ((char *)hexbuf, "<%02X>", (unsigned char) wc);
	     for (jj = 0; jj < 4; jj++)
	       {
		  ADD_CHAR_OR_BREAK (hexbuf[jj]);
	       }
	     continue;
	  }

	width = SLwchar_wcwidth (wc);
	if (width == 0)
	  {
	     /* Combining character--- must follow non-zero width char */
	     if (i == 0)
	       continue;
	     if (i < SLSMG_MAX_CHARS_PER_CELL)
	       {
		  ADD_TO_CHAR_CELL (wc);
	       }
	     continue;
	  }

	if (width == 2)
	  {
	     if (col + 2 <= start_col)
	       {
		  col += 2;
		  continue;
	       }

	     if (col + 2 > max_col)
	       {
		  ADD_CHAR_OR_BREAK('>');
		  break;
	       }
	     
	     if (col == start_col - 1)
	       {
		  /* double width character is clipped at left part of screen.
		   * So, display right edge as a space */
		  col++;
		  ADD_CHAR_OR_BREAK('<');
		  continue;
	       }

	     ADD_CHAR_OR_BREAK(wc);
	     last_was_double_width = 1;
	     continue;
	  }

	ADD_CHAR_OR_BREAK(wc);
     }
   
   if (i != 0)
     {
	NEXT_CHAR_CELL;
     }

   if (last_was_double_width)
     {
	if (col < max_col)
	  NEXT_CHAR_CELL;
	last_was_double_width = 0;
     }
   else if ((col < max_col) && (p->nchars == 0))
     {
	/* The left side of a double with character was overwritten */
	p->nchars = 1;
	p->wchars[0] = ' ';
     }
   SL_Screen[This_Row - Start_Row].flags = flags;
   This_Col = col;

   /* Why would u be NULL here?? */

   if (SLsmg_Newline_Behavior == SLSMG_NEWLINE_IGNORED)
     {
#if SLSMG_HAS_EMBEDDED_ESCAPE
	if (Embedded_Escape_Mode && (u != NULL))
	  parse_embedded_set_color (u, umax, default_color);
#endif
	return;
     }

   if (newline_flag == 0)
     {
#if SLSMG_HAS_EMBEDDED_ESCAPE
	SLuchar_Type *usave = u;
#endif
	if (u == NULL)
	  return;

	while (u < umax)
	  {
	     if (*u == '\n') break;
	     u++;
	  }
	if (u >= umax)
	  {
#if SLSMG_HAS_EMBEDDED_ESCAPE
	     if (Embedded_Escape_Mode)
	       parse_embedded_set_color (usave, umax, default_color);
#endif
	     return;
	  }
	u++;
     }

   This_Row++;
   This_Col = 0;
   if (This_Row == Start_Row + (int)Screen_Rows)
     {
	if (SLsmg_Newline_Behavior == SLSMG_NEWLINE_SCROLLS) scroll_up ();
     }
   goto top;
}


void SLsmg_write_nchars (char *str, unsigned int len)
{
   SLsmg_write_chars ((unsigned char *) str, (unsigned char *)str + len);
}

void SLsmg_write_string (char *str)
{
   SLsmg_write_chars ((unsigned char *)str, 
		      (unsigned char *)str + strlen (str));
}

void SLsmg_write_nstring (char *str, unsigned int n)
{
   unsigned int width;
   unsigned char *blank = (unsigned char *)" ";
   unsigned char *u = (unsigned char *)str;

   /* Avoid a problem if a user accidently passes a negative value */
   if ((int) n < 0)
     return;

   if (u == NULL) width = 0;
   else
     {
	unsigned char *umax;
	
	width = strlen ((char *)u);
	if (UTF8_Mode)
	  umax = SLutf8_skip_chars (u, u+width, n, &width, 0);
	else 
	  {
	     if (width > n) width = n;
	     umax = u + width;
	  }
	SLsmg_write_chars (u, umax);
     }

   while (width++ < n) SLsmg_write_chars (blank, blank+1);
}

void SLsmg_write_wrapped_string (SLuchar_Type *u, int r, int c,
				 unsigned int dr, unsigned int dc,
				 int fill)
{
   int maxc = (int) dc;
   unsigned char *p, *pmax;
   int utf8_mode = UTF8_Mode;

   if ((dr == 0) || (dc == 0)) return;
   p = u;
   pmax = u + strlen ((char *)u);
   
   dc = 0;
   while (1)
     {
	unsigned char ch = *p;
	if ((ch == 0) || (ch == '\n'))
	  {
	     int diff;

	     diff = maxc - (int) dc;

	     SLsmg_gotorc (r, c);
	     SLsmg_write_chars (u, p);
	     if (fill && (diff > 0))
	       {
		  unsigned char *blank = (unsigned char *)" ";
		  while (diff--) SLsmg_write_chars (blank, blank+1);
	       }
	     if ((ch == 0) || (dr == 1)) break;

	     r++;
	     dc = 0;
	     dr--;
	     p++;
	     u = p;
	     continue;
	  }

	if ((int) dc == maxc)
	  {
	     SLsmg_gotorc (r, c);
	     SLsmg_write_chars (u, p);
	     if (dr == 1) break;

	     r++;
	     dc = 0;
	     dr--;
	     u = p;
	     continue;
	  }

	dc++;
	if (utf8_mode)
	  p = SLutf8_skip_chars (p, pmax, 1, NULL, 0);
	else
	  p++;
     }
}

int SLsmg_Tab_Width = 8;

/* Minimum value for which eight bit char is displayed as is. */

#ifndef IBMPC_SYSTEM
int SLsmg_Display_Eight_Bit = 160;
#else
int SLsmg_Display_Eight_Bit = 128;
#endif

void SLsmg_write_char (SLwchar_Type ch)
{
   unsigned char u[SLUTF8_MAX_MBLEN];
   unsigned char *umax;

   if ((ch < 0x80) || (UTF8_Mode == 0))
     {
	u[0] = (unsigned char) ch;
	SLsmg_write_chars (u, u+1);
	return;
     }
   if (NULL == (umax = SLutf8_encode (ch, u, SLUTF8_MAX_MBLEN)))
     return;
   SLsmg_write_chars (u, umax);
}

static int Cls_Flag;

void SLsmg_cls (void)
{
   int tac;
   if (Smg_Inited == 0) return;

   tac = This_Alt_Char; This_Alt_Char = 0;
   SLsmg_set_color (0);
   clear_region (0, (int)Screen_Rows, 0x20);
   This_Alt_Char = tac;
   SLsmg_set_color (0);
   Cls_Flag = 1;
}
#if 0
static void do_copy (SLsmg_Char_Type *a, SLsmg_Char_Type *b)
{
   SLsmg_Char_Type *amax = a + Screen_Cols;

   while (a < amax) *a++ = *b++;
}
#endif

#ifndef IBMPC_SYSTEM
int SLsmg_Scroll_Hash_Border = 0;
static unsigned long compute_hash (SLsmg_Char_Type *c, unsigned int n)
{
   SLsmg_Char_Type *csave, *cmax;
   int is_blank = 2;

   c += SLsmg_Scroll_Hash_Border;
   csave = c;
   cmax = c + (n - SLsmg_Scroll_Hash_Border);

   while ((c < cmax) && is_blank)
     {
	if ((c->wchars[0] != 32) || (c->nchars != 1))
	  is_blank--;
	c++;
     }
   if (is_blank) return 0;

   return _pSLstring_hash ((unsigned char *)csave, (unsigned char *)cmax);
}

static unsigned long Blank_Hash;

static int try_scroll_down (int rmin, int rmax)
{
   int i, r1, r2, di, j;
   unsigned long hash;
   int did_scroll;
   SLsmg_Color_Type color;
   SLsmg_Char_Type *tmp;
   int ignore;

   did_scroll = 0;
   for (i = rmax; i > rmin; i--)
     {
	hash = SL_Screen[i].new_hash;
	if (hash == Blank_Hash) continue;

	if ((hash == SL_Screen[i].old_hash)
#if 0
	    || ((i + 1 < Screen_Rows) && (hash == SL_Screen[i + 1].old_hash))
	    || ((i - 1 > rmin) && (SL_Screen[i].old_hash == SL_Screen[i - 1].new_hash))
#endif
	    )
	  continue;

	for (j = i - 1; j >= rmin; j--)
	  {
	     if (hash == SL_Screen[j].old_hash) break;
	  }
	if (j < rmin) continue;

	r2 = i;			       /* end scroll region */

	di = i - j;
	j--;
	ignore = 0;
	while ((j >= rmin) && (SL_Screen[j].old_hash == SL_Screen[j + di].new_hash))
	  {
	     if (SL_Screen[j].old_hash == Blank_Hash) ignore++;
	     j--;
	  }
	r1 = j + 1;

	/* If this scroll only scrolls this line into place, don't do it.
	 */
	if ((di > 1) && (r1 + di + ignore == r2)) continue;

	/* If there is anything in the scrolling region that is ok, abort the
	 * scroll.
	 */

	for (j = r1; j <= r2; j++)
	  {
	     if ((SL_Screen[j].old_hash != Blank_Hash)
		 && (SL_Screen[j].old_hash == SL_Screen[j].new_hash))
	       {
		  /* See if the scroll is happens to scroll this one into place. */
		  if ((j + di > r2) || (SL_Screen[j].old_hash != SL_Screen[j + di].new_hash))
		    break;
	       }
	  }
	if (j <= r2) continue;

	color = This_Color;  This_Color = 0;
	did_scroll = 1;
	(*tt_normal_video) ();
	(*tt_set_scroll_region) (r1, r2);
	(*tt_goto_rc) (0, 0);
	(*tt_reverse_index) (di);
	(*tt_reset_scroll_region) ();
	/* Now we have a hole in the screen.
	 * Make the virtual screen look like it.
	 * 
	 * Note that if the terminal does not support BCE, then we have
	 * no idea what color the hole is.  So, for this case, we do not
	 * want to add Bce_Color_Offset to This_Color since if Bce_Color_Offset
	 * is non-zero, then This_Color = 0 does not match any valid color
	 * obtained by adding Bce_Color_Offset.
	 */
	for (j = r1; j <= r2; j++) SL_Screen[j].flags = TOUCHED;

	while (di--)
	  {
	     tmp = SL_Screen[r2].old;
	     for (j = r2; j > r1; j--)
	       {
		  SL_Screen[j].old = SL_Screen[j - 1].old;
		  SL_Screen[j].old_hash = SL_Screen[j - 1].old_hash;
	       }
	     SL_Screen[r1].old = tmp;
	     blank_line (SL_Screen[r1].old, Screen_Cols, 0x20);
	     SL_Screen[r1].old_hash = Blank_Hash;
	     r1++;
	  }
	This_Color = color;
     }

   return did_scroll;
}

static int try_scroll_up (int rmin, int rmax)
{
   int i, r1, r2, di, j;
   unsigned long hash;
   int did_scroll;
   SLsmg_Color_Type color;
   SLsmg_Char_Type *tmp;
   int ignore;

   did_scroll = 0;
   for (i = rmin; i < rmax; i++)
     {
	hash = SL_Screen[i].new_hash;
	if (hash == Blank_Hash) continue;
	if (hash == SL_Screen[i].old_hash)
	  continue;
	/* find a match further down screen */
	for (j = i + 1; j <= rmax; j++)
	  {
	     if (hash == SL_Screen[j].old_hash) break;
	  }
	if (j > rmax) continue;

	r1 = i;			       /* beg scroll region */
	di = j - i;		       /* number of lines to scroll */
	j++;			       /* since we know this is a match */

	/* find end of scroll region */
	ignore = 0;
	while ((j <= rmax) && (SL_Screen[j].old_hash == SL_Screen[j - di].new_hash))
	  {
	     if (SL_Screen[j].old_hash == Blank_Hash) ignore++;
	     j++;
	  }
	r2 = j - 1;		       /* end of scroll region */

	/* If this scroll only scrolls this line into place, don't do it.
	 */
	if ((di > 1) && (r1 + di + ignore == r2)) continue;

	/* If there is anything in the scrolling region that is ok, abort the
	 * scroll.
	 */

	for (j = r1; j <= r2; j++)
	  {
	     if ((SL_Screen[j].old_hash != Blank_Hash)
		 && (SL_Screen[j].old_hash == SL_Screen[j].new_hash))
	       {
		  if ((j - di < r1) || (SL_Screen[j].old_hash != SL_Screen[j - di].new_hash))
		    break;
	       }

	  }
	if (j <= r2) continue;

	did_scroll = 1;

	/* See the above comments about BCE */
	color = This_Color;  This_Color = 0;
	(*tt_normal_video) ();
	(*tt_set_scroll_region) (r1, r2);
	(*tt_goto_rc) (0, 0);	       /* relative to scroll region */
	(*tt_delete_nlines) (di);
	(*tt_reset_scroll_region) ();
	/* Now we have a hole in the screen.  Make the virtual screen look
	 * like it.
	 */
	for (j = r1; j <= r2; j++) SL_Screen[j].flags = TOUCHED;

	while (di--)
	  {
	     tmp = SL_Screen[r1].old;
	     for (j = r1; j < r2; j++)
	       {
		  SL_Screen[j].old = SL_Screen[j + 1].old;
		  SL_Screen[j].old_hash = SL_Screen[j + 1].old_hash;
	       }
	     SL_Screen[r2].old = tmp;
	     blank_line (SL_Screen[r2].old, Screen_Cols, ' ');
	     SL_Screen[r2].old_hash = Blank_Hash;
	     r2--;
	  }
	This_Color = color;
     }
   return did_scroll;
}

static void try_scroll (void)
{
   int r1, rmin, rmax;
   int num_up, num_down;
   /* find region limits. */

   for (rmax = Screen_Rows - 1; rmax > 0; rmax--)
     {
	if (SL_Screen[rmax].new_hash != SL_Screen[rmax].old_hash)
	  {
	     r1 = rmax - 1;
	     if ((r1 == 0)
		 || (SL_Screen[r1].new_hash != SL_Screen[r1].old_hash))
	       break;

	     rmax = r1;
	  }
     }

   for (rmin = 0; rmin < rmax; rmin++)
     {
	if (SL_Screen[rmin].new_hash != SL_Screen[rmin].old_hash)
	  {
	     r1 = rmin + 1;
	     if ((r1 == rmax)
		 || (SL_Screen[r1].new_hash != SL_Screen[r1].old_hash))
	       break;

	     rmin = r1;
	  }
     }

   /* Below, we have two scrolling algorithms.  The first has the effect of
    * scrolling lines down.  This is usually appropriate when one moves
    * up the display, e.g., with the UP arrow.  The second algorithm is
    * appropriate for going the other way.  It is important to choose the
    * correct one.
    */

   num_up = 0;
   for (r1 = rmin; r1 < rmax; r1++)
     {
	if (SL_Screen[r1].new_hash == SL_Screen[r1 + 1].old_hash)
	  num_up++;
     }

   num_down = 0;
   for (r1 = rmax; r1 > rmin; r1--)
     {
	if (SL_Screen[r1 - 1].old_hash == SL_Screen[r1].new_hash)
	  num_down++;
     }

   if (num_up > num_down)
     {
	if (try_scroll_up (rmin, rmax))
	  return;

	(void) try_scroll_down (rmin, rmax);
     }
   else
     {
	if (try_scroll_down (rmin, rmax))
	  return;

	(void) try_scroll_up (rmin, rmax);
     }
}
#endif   /* NOT IBMPC_SYSTEM */

#ifdef REQUIRES_NON_BCE_SUPPORT
static void adjust_colors (void)
{
   int bce;
   unsigned int i;

   bce = Bce_Color_Offset;
   Bce_Color_Offset = _pSLtt_get_bce_color_offset ();
   if (bce == Bce_Color_Offset)
     return;

   for (i = 0; i < Screen_Rows; i++)
     {
	SLsmg_Char_Type *s, *smax;

	SL_Screen[i].flags |= TRASHED;
	s = SL_Screen[i].neew;
	smax = s + Screen_Cols;

	while (s < smax)
	  {
	     SLsmg_Color_Type color = s->color;
	     int acs = color & SLSMG_ACS_MASK;
	     color = (color & SLSMG_COLOR_MASK) + (Bce_Color_Offset - bce);
	     if (color < SLSMG_MAX_COLORS)
	       s->color = color | acs;

	     s++;
	  }
     }
}
#endif

void SLsmg_refresh (void)
{
   unsigned int i;
#ifndef IBMPC_SYSTEM
   int trashed = 0;
#endif
   int r, c;

   if (Smg_Inited == 0) return;
   
   if (Screen_Trashed)
     {
	Cls_Flag = 1;
	for (i = 0; i < Screen_Rows; i++)
	  SL_Screen[i].flags |= TRASHED;
#ifdef REQUIRES_NON_BCE_SUPPORT
	adjust_colors ();
#endif
     }

#ifndef IBMPC_SYSTEM
   for (i = 0; i < Screen_Rows; i++)
     {
	if (SL_Screen[i].flags == 0) continue;
	SL_Screen[i].new_hash = compute_hash (SL_Screen[i].neew, Screen_Cols);
	trashed = 1;
     }
#endif

   if (Cls_Flag)
     {
	(*tt_normal_video) ();  (*tt_cls) ();
     }
#ifndef IBMPC_SYSTEM
   else if (trashed && (*tt_Term_Cannot_Scroll == 0)) try_scroll ();
#endif

   for (i = 0; i < Screen_Rows; i++)
     {
	if (SL_Screen[i].flags == 0) continue;

	if (Cls_Flag || SL_Screen[i].flags & TRASHED)
	  {
	     SLsmg_Color_Type color = This_Color;

	     if (Cls_Flag == 0) 
	       {
		  (*tt_goto_rc) (i, 0);
		  (*tt_del_eol) ();
	       }
	     This_Color = 0;
	     blank_line (SL_Screen[i].old, Screen_Cols, 0x20);
	     This_Color = color;
	  }

	(*tt_smart_puts) (SL_Screen[i].neew, SL_Screen[i].old, Screen_Cols, i);

	memcpy ((char *) SL_Screen[i].old, (char *) SL_Screen[i].neew,
		Screen_Cols * sizeof (SLsmg_Char_Type));

	SL_Screen[i].flags = 0;
#ifndef IBMPC_SYSTEM
	SL_Screen[i].old_hash = SL_Screen[i].new_hash;
#endif
     }


   r = This_Row - Start_Row;
   c = This_Col - Start_Col;
   if (r < 0) 
     {
	r = 0; 
	c = 0;
     }
   else if (r >= (int)Screen_Rows)
     {
	r = (int)Screen_Rows;
	c = (int)Screen_Cols-1;
     }
   if (c < 0) 
     c = 0;
   else if (c >= (int)Screen_Cols)
     c = (int)Screen_Cols-1;

   (*tt_goto_rc) (r,c);
   (void) (*tt_flush_output) ();
   Cls_Flag = 0;
   Screen_Trashed = 0;
}

static int compute_clip (int row, int n, int box_start, int box_end,
			 int *rmin, int *rmax)
{
   int row_max;

   if (n < 0) return 0;
   if (row >= box_end) return 0;
   row_max = row + n;
   if (row_max <= box_start) return 0;

   if (row < box_start) row = box_start;
   if (row_max >= box_end) row_max = box_end;
   *rmin = row;
   *rmax = row_max;
   return 1;
}

void SLsmg_touch_lines (int row, unsigned int n)
{
   int i;
   int r1, r2;

   /* Allow this function to be called even when we are not initialied.
    * Calling this function is useful after calling SLtt_set_color
    * to force the display to be redrawn
    */

   if (Smg_Inited == 0)
     return;

   if (0 == compute_clip (row, (int) n, Start_Row, Start_Row + Screen_Rows, &r1, &r2))
     return;

   r1 -= Start_Row;
   r2 -= Start_Row;
   for (i = r1; i < r2; i++)
     {
	SL_Screen[i].flags |= TRASHED;
     }
}

void SLsmg_touch_screen (void)
{
   Screen_Trashed = 1;
}

#ifndef IBMPC_SYSTEM
# define BLOCK_SIGNALS SLsig_block_signals ()
# define UNBLOCK_SIGNALS SLsig_unblock_signals ()
#else
# define BLOCK_SIGNALS (void)0
# define UNBLOCK_SIGNALS (void)0
#endif

static int Smg_Suspended;
int SLsmg_suspend_smg (void)
{
   BLOCK_SIGNALS;

   if (Smg_Suspended == 0)
     {
	(*tt_reset_video) ();
	Smg_Suspended = 1;
     }

   UNBLOCK_SIGNALS;
   return 0;
}

int SLsmg_resume_smg (void)
{
   BLOCK_SIGNALS;

   if (Smg_Suspended == 0)
     {
	UNBLOCK_SIGNALS;
	return 0;
     }

   Smg_Suspended = 0;

   if (-1 == (*tt_init_video) ())
     {
	UNBLOCK_SIGNALS;
	return -1;
     }

   Cls_Flag = 1;
   SLsmg_touch_screen ();
   SLsmg_refresh ();

   UNBLOCK_SIGNALS;
   return 0;
}

   
static void reset_smg (void)
{
   unsigned int i;
   if (Smg_Inited == 0)
     return;

   for (i = 0; i < Screen_Rows; i++)
     {
	SLfree ((char *)SL_Screen[i].old);
	SLfree ((char *)SL_Screen[i].neew);
	SL_Screen[i].old = SL_Screen[i].neew = NULL;
     }
   This_Alt_Char = This_Color = 0;
   Smg_Inited = 0;
}


static int init_smg (void)
{
   unsigned int i, len;
   SLsmg_Char_Type *old, *neew;

   Smg_Inited = 0;

#ifdef REQUIRES_NON_BCE_SUPPORT
   Bce_Color_Offset = _pSLtt_get_bce_color_offset ();
#endif
   
   Screen_Rows = *tt_Screen_Rows;
   if (Screen_Rows > SLTT_MAX_SCREEN_ROWS)
     Screen_Rows = SLTT_MAX_SCREEN_ROWS;

   Screen_Cols = *tt_Screen_Cols;

   This_Col = This_Row = Start_Col = Start_Row = 0;

   This_Alt_Char = 0;
   SLsmg_set_color (0);
   Cls_Flag = 1;
   
   init_acs (ACS_MODE_AUTO);

   len = Screen_Cols + 3;
   for (i = 0; i < Screen_Rows; i++)
     {
	if ((NULL == (old = (SLsmg_Char_Type *) SLmalloc (sizeof(SLsmg_Char_Type) * len)))
	    || ((NULL == (neew = (SLsmg_Char_Type *) SLmalloc (sizeof(SLsmg_Char_Type) * len)))))
	  {
	     SLfree ((char *) old);
	     return -1;
	  }
	blank_line (old, len, ' ');
	blank_line (neew, len, ' ');
	SL_Screen[i].old = old;
	SL_Screen[i].neew = neew;
	SL_Screen[i].flags = 0;
#ifndef IBMPC_SYSTEM
	Blank_Hash = compute_hash (old, Screen_Cols);
	SL_Screen[i].new_hash = SL_Screen[i].old_hash =  Blank_Hash;
#endif
     }
   
   _pSLtt_color_changed_hook = SLsmg_touch_screen;
   Screen_Trashed = 1;
   Smg_Inited = 1;
   return 0;
}


int SLsmg_init_smg (void)
{
   int ret;

   BLOCK_SIGNALS;

   if (tt_Screen_Rows == NULL)
     init_tt_symbols ();

   if (Smg_Inited)
     SLsmg_reset_smg ();

   if (UTF8_Mode == -1)
     UTF8_Mode = _pSLutf8_mode;

   if (-1 == (*tt_init_video) ())
     {
	UNBLOCK_SIGNALS;
	return -1;
     }
   
   if (-1 == (ret = init_smg ()))
     (void) (*tt_reset_video)();

   UNBLOCK_SIGNALS;
   return ret;
}

int SLsmg_reinit_smg (void)
{
   int ret;

   if (Smg_Inited == 0)
     return SLsmg_init_smg ();

   BLOCK_SIGNALS;
   reset_smg ();
   ret = init_smg ();
   UNBLOCK_SIGNALS;
   return ret;
}

void SLsmg_reset_smg (void)
{   
   if (Smg_Inited == 0)
     return;
   
   BLOCK_SIGNALS;

   reset_smg ();
   (*tt_reset_video)();

   UNBLOCK_SIGNALS;
}

void SLsmg_vprintf (char *fmt, va_list ap)
{
   char buf[1024];

   if (Smg_Inited == 0) return;

   (void) SLvsnprintf (buf, sizeof (buf), fmt, ap);
   SLsmg_write_string (buf);
}

void SLsmg_printf (char *fmt, ...)
{
   va_list ap;
   char *f;

   if (Smg_Inited == 0) return;

   va_start(ap, fmt);

   f = fmt;
   while (*f && (*f != '%'))
     f++;
   if (f != fmt)
     SLsmg_write_chars ((SLuchar_Type *)fmt, (SLuchar_Type *)f);

   if (*f != 0)
     SLsmg_vprintf (f, ap);

   va_end (ap);
}

void SLsmg_set_screen_start (int *r, int *c)
{
   int orow = Start_Row, oc = Start_Col;

   if (Smg_Inited == 0) return;

   if (c == NULL) Start_Col = 0;
   else
     {
	Start_Col = *c;
	*c = oc;
     }
   if (r == NULL) Start_Row = 0;
   else
     {
	Start_Row = *r;
	*r = orow;
     }
}

void SLsmg_draw_object (int r, int c, SLwchar_Type object)
{
   This_Row = r;  This_Col = c;

   if (Smg_Inited == 0) return;

   if (point_visible (1))
     {
	int color = This_Color;
	This_Color |= SLSMG_ACS_MASK;
	SLsmg_write_char (object);
	This_Color = color;
     }

   This_Col = c + 1;
}

void SLsmg_draw_hline (unsigned int n)
{
   static unsigned char hbuf[16];
   int count;
   int cmin, cmax;
   int final_col = This_Col + (int) n;
   int save_color;

   if (Smg_Inited == 0) return;

   if ((This_Row < Start_Row) || (This_Row >= Start_Row + (int)Screen_Rows)
       || (0 == compute_clip (This_Col, n, Start_Col, Start_Col + (int)Screen_Cols,
			      &cmin, &cmax)))
     {
	This_Col = final_col;
	return;
     }

   n = (unsigned int)(cmax - cmin);
   count = n / 16;

   save_color = This_Color;
   This_Color |= SLSMG_ACS_MASK;
   This_Col = cmin;

      
   if (hbuf[0] == 0)
     {
	SLMEMSET ((char *) hbuf, SLSMG_HLINE_CHAR, 16);
     }


   while (n)
     {
	SLsmg_write_char (SLSMG_HLINE_CHAR);
	n--;
     }
   This_Color = save_color;
   This_Col = final_col;
}

void SLsmg_draw_vline (int n)
{
   int c = This_Col, rmin, rmax;
   int final_row = This_Row + n;
   int save_color;

   if (Smg_Inited == 0) return;

   if (((c < Start_Col) || (c >= Start_Col + (int)Screen_Cols)) 
       || (0 == compute_clip (This_Row, n, Start_Row, 
			      Start_Row + (int)Screen_Rows,
			      &rmin, &rmax)))
     {
	This_Row = final_row;
	return;
     }

   save_color = This_Color;
   This_Color |= SLSMG_ACS_MASK;

   for (This_Row = rmin; This_Row < rmax; This_Row++)
     {
	This_Col = c;
	SLsmg_write_char (SLSMG_VLINE_CHAR);
     }

   This_Col = c;  This_Row = final_row;
   This_Color = save_color;
}

void SLsmg_draw_box (int r, int c, unsigned int dr, unsigned int dc)
{
   if (Smg_Inited == 0) return;

   if (!dr || !dc) return;
   This_Row = r;  This_Col = c;
   dr--; dc--;
   SLsmg_draw_hline (dc);
   SLsmg_draw_vline (dr);
   This_Row = r;  This_Col = c;
   SLsmg_draw_vline (dr);
   SLsmg_draw_hline (dc);
   SLsmg_draw_object (r, c, SLSMG_ULCORN_CHAR);
   SLsmg_draw_object (r, c + (int) dc, SLSMG_URCORN_CHAR);
   SLsmg_draw_object (r + (int) dr, c, SLSMG_LLCORN_CHAR);
   SLsmg_draw_object (r + (int) dr, c + (int) dc, SLSMG_LRCORN_CHAR);
   This_Row = r; This_Col = c;
}

void SLsmg_fill_region (int r, int c, unsigned int dr, unsigned int dc, SLwchar_Type wch)
{
   static unsigned char buf[16];
   unsigned char ubuf[16*SLUTF8_MAX_MBLEN];
   unsigned char *b, *bmax;
   int count;
   int dcmax, rmax;
   unsigned int wchlen;

   if (Smg_Inited == 0) return;

   SLsmg_gotorc (r, c);
   r = This_Row; c = This_Col;

   dcmax = Screen_Cols - This_Col;
   if (dcmax < 0)
     return;

   if (dc > (unsigned int) dcmax) dc = (unsigned int) dcmax;

   rmax = This_Row + (int)dr;
   if (rmax > (int)Screen_Rows) rmax = (int)Screen_Rows;

   if ((wch < 0x80) 
       || (UTF8_Mode == 0))
     {
	if (buf[0] != (unsigned char) wch)
	  memset ((char *) buf, (unsigned char) wch, sizeof(buf));
	b = buf;
	bmax = buf + sizeof (buf);
	wchlen = 1;
     }
   else
     {
	unsigned int i;

	b = ubuf;
	bmax = SLutf8_encode (wch, b, SLUTF8_MAX_MBLEN);
	if (bmax == NULL)
	  {
	     bmax = ubuf;
	     *bmax++ = '?';
	  }
	wchlen = (unsigned int) (bmax - b);
	for (i = 1; i < 16; i++)
	  {
	     memcpy (bmax, b, wchlen);
	     bmax += wchlen;
	  }
     }

   for (This_Row = r; This_Row < rmax; This_Row++)
     {
	This_Col = c;
	count = dc / 16;
	SLsmg_write_chars (b, b + wchlen * (dc % 16));
	while (count-- > 0)
	  {
	     SLsmg_write_chars (b, bmax);
	  }
     }

   This_Row = r;
}

void SLsmg_forward (int n)
{
   This_Col += n;
}

void
SLsmg_set_color_in_region (int color, int r, int c, unsigned int dr, unsigned int dc)
{
   int cmax, rmax;

   if (Smg_Inited == 0) return;

   c -= Start_Col;
   r -= Start_Row;

   cmax = c + (int) dc;
   rmax = r + (int) dr;

   if (cmax > (int)Screen_Cols) cmax = (int)Screen_Cols;
   if (rmax > (int)Screen_Rows) rmax = (int)Screen_Rows;

   if (c < 0) c = 0;
   if (r < 0) r = 0;

#ifdef REQUIRES_NON_BCE_SUPPORT
   if (Bce_Color_Offset)
     color += Bce_Color_Offset;
#endif

   while (r < rmax)
     {
	SLsmg_Char_Type *cell, *cell_max;

	SL_Screen[r].flags |= TOUCHED;
	cell = SL_Screen[r].neew;
	cell_max = cell + cmax;
	cell += c;

	while (cell < cell_max)
	  {
	     int acs = cell->color & SLSMG_ACS_MASK;
	     cell->color = color | acs;
	     cell++;
	  }
	r++;
     }
}

void SLsmg_set_terminal_info (SLsmg_Term_Type *tt)
{
   if (tt == NULL)		       /* use default */
     return;

   if ((tt->tt_normal_video == NULL)
       || (tt->tt_goto_rc == NULL)
       || (tt->tt_cls == NULL)
       || (tt->tt_del_eol == NULL)
       || (tt->tt_smart_puts == NULL)
       || (tt->tt_flush_output == NULL)
       || (tt->tt_reset_video == NULL)
       || (tt->tt_init_video == NULL)
#ifndef IBMPC_SYSTEM
       || (tt->tt_set_scroll_region == NULL)
       || (tt->tt_reverse_index == NULL)
       || (tt->tt_reset_scroll_region == NULL)
       || (tt->tt_delete_nlines == NULL)
       /* Variables */
       || (tt->tt_term_cannot_scroll == NULL)
       || (tt->tt_has_alt_charset == NULL)
#if 0 /* These can be NULL */
       || (tt->tt_use_blink_for_acs == NULL)
       || (tt->tt_graphic_char_pairs == NULL)
#endif
       || (tt->tt_screen_cols == NULL)
       || (tt->tt_screen_rows == NULL)
#endif
       )
     SLang_exit_error ("The Terminal not powerful enough for S-Lang's SLsmg interface");

   tt_normal_video = tt->tt_normal_video;
   tt_goto_rc = tt->tt_goto_rc;
   tt_cls = tt->tt_cls;
   tt_del_eol = tt->tt_del_eol;
   tt_smart_puts = tt->tt_smart_puts;
   tt_flush_output = tt->tt_flush_output;
   tt_reset_video = tt->tt_reset_video;
   tt_init_video = tt->tt_init_video;

#ifndef IBMPC_SYSTEM
   tt_set_scroll_region = tt->tt_set_scroll_region;
   tt_reverse_index = tt->tt_reverse_index;
   tt_reset_scroll_region = tt->tt_reset_scroll_region;
   tt_delete_nlines = tt->tt_delete_nlines;

   tt_Term_Cannot_Scroll = tt->tt_term_cannot_scroll;
#endif

   tt_Has_Alt_Charset = tt->tt_has_alt_charset;
   tt_Screen_Cols = tt->tt_screen_cols;
   tt_Screen_Rows = tt->tt_screen_rows;
   tt_unicode_ok = tt->unicode_ok;
}

/* The following routines are partially supported. */
void SLsmg_write_color_chars (SLsmg_Char_Type *s, unsigned int len)
{
#if 1
   SLsmg_write_raw (s, len);
#else
   SLsmg_Char_Type *smax, sh;
   char buf[32], *b, *bmax;
   int color, save_color;

   if (Smg_Inited == 0) return;

   smax = s + len;
   b = buf;
   bmax = b + sizeof (buf);

   save_color = This_Color;

   while (s < smax)
     {
	sh = *s++;

	color = SLSMG_EXTRACT_COLOR(sh);

#ifdef REQUIRES_NON_BCE_SUPPORT
	if (Bce_Color_Offset)
	  {
	     if (color & 0x80)
	       color = ((color & 0x7F) + Bce_Color_Offset) | 0x80;
	     else
	       color = ((color & 0x7F) + Bce_Color_Offset) & 0x7F;
	  }
#endif

	if ((color != This_Color) || (b == bmax))
	  {
	     if (b != buf)
	       {
		  SLsmg_write_nchars (buf, (int) (b - buf));
		  b = buf;
	       }
	     This_Color = color;
	  }
	*b++ = (char) SLSMG_EXTRACT_CHAR(sh);
     }

   if (b != buf)
     SLsmg_write_nchars (buf, (unsigned int) (b - buf));

   This_Color = save_color;
#endif
}

unsigned int SLsmg_strwidth (SLuchar_Type *u, SLuchar_Type *umax)
{
   unsigned char display_8bit;
   int utf8_mode = UTF8_Mode;
   int col;

   if (u == NULL)
     return 0;

   display_8bit = (unsigned char) SLsmg_Display_Eight_Bit;
   if (utf8_mode)
     display_8bit = 0xA0;

   col = This_Col;

   while (u < umax)
     {
	SLuchar_Type ch;
	unsigned int nconsumed;
	SLwchar_Type wc;

	ch = *u;
	if (ch < 0x80)
	  {
	     u++;

	     if ((ch >= 0x20) && (ch != 0x7F))
	       {
		  col++;
		  continue;
	       }
	     
	     if ((ch == '\t') && (SLsmg_Tab_Width > 0))
	       {
		  if (col >= 0)
		    col = (1 + col/SLsmg_Tab_Width) * SLsmg_Tab_Width;
		  else
		    col = ((col + 1)/SLsmg_Tab_Width) * SLsmg_Tab_Width;
		  
		  continue;
	       }

	     if ((ch == '\n')
		 && (SLsmg_Newline_Behavior != SLSMG_NEWLINE_PRINTABLE))
	       break;
	     
	     if ((ch == 0x8) && SLsmg_Backspace_Moves)
	       {
		  col--;
		  continue;
	       }
#if SLSMG_HAS_EMBEDDED_ESCAPE
	     if ((ch == 033) && Embedded_Escape_Mode)
	       {
		  SLsmg_Color_Type color;
		  if (0 == parse_embedded_escape (u, umax, 0, &u, &color))
		    continue;
	       }
#endif
	     col += 2;
	     continue;
	  }
	
	nconsumed = 1;
	if ((utf8_mode == 0)
	    || (NULL == SLutf8_decode (u, umax, &wc, &nconsumed)))
	  {	     
	     if ((utf8_mode == 0)
		 && (display_8bit && (*u >= display_8bit)))
	       col++;
	     else
	       col += 4*nconsumed;
	     u += nconsumed;
	     continue;
	  }

	u += nconsumed;
	if (wc < (SLwchar_Type)display_8bit)
	  {
	     col += 4;
	     continue;
	  }
	col += SLwchar_wcwidth (wc);
     }
   
   if (col < This_Col)
     return 0;

   return (unsigned int) (col - This_Col);
}

/* If the string u were written at the current positition, this function 
 * returns the number of bytes necessary to reach the specified width.
 */
unsigned int SLsmg_strbytes (SLuchar_Type *u, SLuchar_Type *umax, unsigned int width)
{
   SLuchar_Type *ustart;
   unsigned char display_8bit;
   int utf8_mode = UTF8_Mode;
   int col, col_max;

   if (u == NULL)
     return 0;

   display_8bit = (unsigned char) SLsmg_Display_Eight_Bit;
   if (utf8_mode)
     display_8bit = 0xA0;

   col = This_Col;
   col_max = col + width;
   ustart = u;

   while (u < umax)
     {
	SLuchar_Type ch;
	SLwchar_Type wc;
	unsigned int nconsumed = 1;

	ch = *u;
	if (ch < 0x80)
	  {
	     if ((ch >= 0x20) && (ch != 0x7F))
	       col++;
	     else if ((ch == '\t') && (SLsmg_Tab_Width > 0))
	       {
		  if (col >= 0)
		    col = (1 + col/SLsmg_Tab_Width) * SLsmg_Tab_Width;
		  else
		    col = ((col + 1)/SLsmg_Tab_Width) * SLsmg_Tab_Width;
	       }
	     else if ((ch == '\n')
		      && (SLsmg_Newline_Behavior != SLSMG_NEWLINE_PRINTABLE))
	       break;
	     else if ((ch == 0x8) && SLsmg_Backspace_Moves)
	       col--;
#if SLSMG_HAS_EMBEDDED_ESCAPE
	     else if ((ch == 033) && Embedded_Escape_Mode)
	       {
		  SLsmg_Color_Type color;
		  SLuchar_Type *u1 = u+1;
		  if (-1 == parse_embedded_escape (u1, umax, 0, &u1, &color))
		    col += 2;
		  nconsumed = (u1 - u);
	       }
#endif
	     else col += 2;
	  }
	else if ((utf8_mode == 0)
		 || (NULL == SLutf8_decode (u, umax, &wc, &nconsumed)))
	  {
	     if ((utf8_mode == 0)
		 && (display_8bit && (*u >= display_8bit)))
	       col++;
	     else
	       col += 4*nconsumed;     /* <XX> */
	  }
	else if (wc < (SLwchar_Type)display_8bit)
	  col += 4;
	else col += SLwchar_wcwidth (wc);
	
	if (col >= col_max)
	  break;
	
	u += nconsumed;
     }

   return (unsigned int) (u - ustart);
}

unsigned int SLsmg_read_raw (SLsmg_Char_Type *buf, unsigned int len)
{
   unsigned int r, c;

   if (Smg_Inited == 0) return 0;

   if (0 == point_visible (1)) return 0;

   r = (unsigned int) (This_Row - Start_Row);
   c = (unsigned int) (This_Col - Start_Col);

   if (c + len > (unsigned int) Screen_Cols)
     len = (unsigned int) Screen_Cols - c;

   memcpy ((char *) buf, (char *) (SL_Screen[r].neew + c), len * sizeof (SLsmg_Char_Type));
   return len;
}

unsigned int SLsmg_write_raw (SLsmg_Char_Type *buf, unsigned int len)
{
   unsigned int r, c;
   SLsmg_Char_Type *dest;

   if (Smg_Inited == 0) return 0;

   if (0 == point_visible (1)) return 0;

   r = (unsigned int) (This_Row - Start_Row);
   c = (unsigned int) (This_Col - Start_Col);

   if (c + len > (unsigned int) Screen_Cols)
     len = (unsigned int) Screen_Cols - c;

   dest = SL_Screen[r].neew + c;

   if (0 != memcmp ((char *) dest, (char *) buf, len * sizeof (SLsmg_Char_Type)))
     {
	memcpy ((char *) dest, (char *) buf, len * sizeof (SLsmg_Char_Type));
	SL_Screen[r].flags |= TOUCHED;
     }
   return len;
}

int SLsmg_char_at (SLsmg_Char_Type *cp)
{
   if (Smg_Inited == 0) return -1;

   if (point_visible (1))
     {
	SLsmg_Char_Type *c = &SL_Screen[This_Row - Start_Row].neew[This_Col - Start_Col];
	if (c->nchars == 0)
	  return -1;
	*cp = *c;
	return 0;
     }
   return -1;
}

int SLsmg_utf8_enable (int mode)
{
   if (mode == -1)
     mode = _pSLutf8_mode;
   
   return UTF8_Mode = mode;
}

int SLsmg_is_utf8_mode (void)
{
   int mode = UTF8_Mode;
   if (mode == -1)
     mode = _pSLutf8_mode;

   return mode;
}
