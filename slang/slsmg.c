/* SLang Screen management routines */
/* Copyright (c) 1992, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

typedef struct Screen_Type
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

#if !defined(__MSDOS_16BIT__)
# define MAX_SCREEN_SIZE 256
#else
# define MAX_SCREEN_SIZE 75
#endif

Screen_Type SL_Screen[MAX_SCREEN_SIZE];
static int Start_Col, Start_Row;
static int Screen_Cols, Screen_Rows;
static int This_Row, This_Col;
static int This_Color;		       /* only the first 8 bits of this
					* are used.  The highest bit is used
					* to indicate an alternate character
					* set.  This leaves 127 userdefineable
					* color combination.
					*/

#ifndef IBMPC_SYSTEM
#define ALT_CHAR_FLAG 0x80
#else
#define ALT_CHAR_FLAG 0x00
#endif

#if SLTT_HAS_NON_BCE_SUPPORT && !defined(IBMPC_SYSTEM)
#define REQUIRES_NON_BCE_SUPPORT 1
static int Bce_Color_Offset;
#endif

int SLsmg_Newline_Behavior = 0;
int SLsmg_Backspace_Moves = 0;
/* Backward compatibility. Not used. */
/* int SLsmg_Newline_Moves; */

static void (*tt_normal_video)(void) = SLtt_normal_video;
static void (*tt_goto_rc)(int, int) = SLtt_goto_rc;
static void (*tt_cls) (void) = SLtt_cls;
static void (*tt_del_eol) (void) = SLtt_del_eol;
static void (*tt_smart_puts) (SLsmg_Char_Type *, SLsmg_Char_Type *, int, int) = SLtt_smart_puts;
static int (*tt_flush_output) (void) = SLtt_flush_output;
static int (*tt_reset_video) (void) = SLtt_reset_video;
static int (*tt_init_video) (void) = SLtt_init_video;
static int *tt_Screen_Rows = &SLtt_Screen_Rows;
static int *tt_Screen_Cols = &SLtt_Screen_Cols;

#ifndef IBMPC_SYSTEM
static void (*tt_set_scroll_region)(int, int) = SLtt_set_scroll_region;
static void (*tt_reverse_index)(int) = SLtt_reverse_index;
static void (*tt_reset_scroll_region)(void) = SLtt_reset_scroll_region;
static void (*tt_delete_nlines)(int) = SLtt_delete_nlines;
#endif

#ifndef IBMPC_SYSTEM
static int *tt_Term_Cannot_Scroll = &SLtt_Term_Cannot_Scroll;
static int *tt_Has_Alt_Charset = &SLtt_Has_Alt_Charset;
static char **tt_Graphics_Char_Pairs = &SLtt_Graphics_Char_Pairs;
static int *tt_Use_Blink_For_ACS = &SLtt_Use_Blink_For_ACS;
#endif

static int Smg_Inited;

static void blank_line (SLsmg_Char_Type *p, int n, unsigned char ch)
{
   register SLsmg_Char_Type *pmax = p + n;
   register SLsmg_Char_Type color_ch;

   color_ch = SLSMG_BUILD_CHAR(ch,This_Color);

   while (p < pmax)
     {
	*p++ = color_ch;
     }
}

static void clear_region (int row, int n)
{
   int i;
   int imax = row + n;

   if (imax > Screen_Rows) imax = Screen_Rows;
   for (i = row; i < imax; i++)
     {
	if (i >= 0)
	  {
	     blank_line (SL_Screen[i].neew, Screen_Cols, ' ');
	     SL_Screen[i].flags |= TOUCHED;
	  }
     }
}

void SLsmg_erase_eol (void)
{
   int r, c;

   if (Smg_Inited == 0) return;

   c = This_Col - Start_Col;
   r = This_Row - Start_Row;

   if ((r < 0) || (r >= Screen_Rows)) return;
   if (c < 0) c = 0; else if (c >= Screen_Cols) return;
   blank_line (SL_Screen[This_Row].neew + c , Screen_Cols - c, ' ');
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
   blank_line (neew, Screen_Cols, ' ');
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
   clear_region (This_Row + 1, Screen_Rows);
}

static int This_Alt_Char;

void SLsmg_set_char_set (int i)
{
#ifdef IBMPC_SYSTEM
   (void) i;
#else
   if ((tt_Use_Blink_For_ACS != NULL)
       && (*tt_Use_Blink_For_ACS != 0))
     return;/* alt chars not used and the alt bit
	     * is used to indicate a blink.
	     */

   if (i) This_Alt_Char = ALT_CHAR_FLAG;
   else This_Alt_Char = 0;

   This_Color &= 0x7F;
   This_Color |= This_Alt_Char;
#endif
}

void SLsmg_set_color (int color)
{
   if (color < 0) return;
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
   return ((This_Row >= Start_Row) && (This_Row < Start_Row + Screen_Rows)
	   && ((col_too == 0)
	       || ((This_Col >= Start_Col)
		   && (This_Col < Start_Col + Screen_Cols))));
}

void SLsmg_write_string (const char *str)
{
   SLsmg_write_nchars (str, strlen (str));
}

void SLsmg_write_nstring (char *str, unsigned int n)
{
   unsigned int width;
   char blank = ' ';

   /* Avoid a problem if a user accidently passes a negative value */
   if ((int) n < 0)
     return;

   if (str == NULL) width = 0;
   else
     {
	width = strlen (str);
	if (width > n) width = n;
	SLsmg_write_nchars (str, width);
     }
   while (width++ < n) SLsmg_write_nchars (&blank, 1);
}

void SLsmg_write_wrapped_string (char *s, int r, int c,
				 unsigned int dr, unsigned int dc,
				 int fill)
{
   register char ch, *p;
   int maxc = (int) dc;

   if ((dr == 0) || (dc == 0)) return;
   p = s;
   dc = 0;
   while (1)
     {
	ch = *p++;
	if ((ch == 0) || (ch == '\n'))
	  {
	     int diff;

	     diff = maxc - (int) dc;

	     SLsmg_gotorc (r, c);
	     SLsmg_write_nchars (s, dc);
	     if (fill && (diff > 0))
	       {
		  while (diff--) SLsmg_write_char (' ');
	       }
	     if ((ch == 0) || (dr == 1)) break;

	     r++;
	     dc = 0;
	     dr--;
	     s = p;
	  }
	else if ((int) dc == maxc)
	  {
	     SLsmg_gotorc (r, c);
	     SLsmg_write_nchars (s, dc + 1);
	     if (dr == 1) break;

	     r++;
	     dc = 0;
	     dr--;
	     s = p;
	  }
	else dc++;
     }
}

int SLsmg_Tab_Width = 8;

/* Minimum value for which eight bit char is displayed as is. */

#ifndef IBMPC_SYSTEM
int SLsmg_Display_Eight_Bit = 160;
static unsigned char Alt_Char_Set[129];/* 129th is used as a flag */
#else
int SLsmg_Display_Eight_Bit = 128;
#endif

void SLsmg_write_nchars (const char *str, unsigned int n)
{
   register SLsmg_Char_Type *p, old, neew, color;
   unsigned char ch;
   unsigned int flags;
   int len, start_len, max_len;
   char *str_max;
   int newline_flag;
#ifndef IBMPC_SYSTEM
   int alt_char_set_flag;

   alt_char_set_flag = ((This_Color & ALT_CHAR_FLAG)
			&& ((tt_Use_Blink_For_ACS == NULL)
			    || (*tt_Use_Blink_For_ACS == 0)));
#endif

   if (Smg_Inited == 0) return;

   str_max = str + n;
   color = This_Color;

   top:				       /* get here only on newline */

   newline_flag = 0;
   start_len = Start_Col;

   if (point_visible (0) == 0) return;

   len = This_Col;
   max_len = start_len + Screen_Cols;

   p = SL_Screen[This_Row - Start_Row].neew;
   if (len > start_len) p += (len - start_len);

   flags = SL_Screen[This_Row - Start_Row].flags;
   while ((len < max_len) && (str < str_max))
     {
	ch = (unsigned char) *str++;

#ifndef IBMPC_SYSTEM
	if (alt_char_set_flag)
	  ch = Alt_Char_Set [ch & 0x7F];
#endif
	if (((ch >= ' ') && (ch < 127))
	    || (ch >= (unsigned char) SLsmg_Display_Eight_Bit)
#ifndef IBMPC_SYSTEM
	    || alt_char_set_flag
#endif
	    )
	  {
	     len += 1;
	     if (len > start_len)
	       {
		  old = *p;
		  neew = SLSMG_BUILD_CHAR(ch,color);
		  if (old != neew)
		    {
		       flags |= TOUCHED;
		       *p = neew;
		    }
		  p++;
	       }
	  }

	else if ((ch == '\t') && (SLsmg_Tab_Width > 0))
	  {
	     n = len;
	     n += SLsmg_Tab_Width;
	     n = SLsmg_Tab_Width - (n % SLsmg_Tab_Width);
	     if ((unsigned int) len + n > (unsigned int) max_len)
	       n = (unsigned int) (max_len - len);
	     neew = SLSMG_BUILD_CHAR(' ',color);
	     while (n--)
	       {
		  len += 1;
		  if (len > start_len)
		    {
		       if (*p != neew)
			 {
			    flags |= TOUCHED;
			    *p = neew;
			 }
		       p++;
		    }
	       }
	  }
	else if ((ch == '\n')
		 && (SLsmg_Newline_Behavior != SLSMG_NEWLINE_PRINTABLE))
	  {
	     newline_flag = 1;
	     break;
	  }
	else if ((ch == 0x8) && SLsmg_Backspace_Moves)
	  {
	     if (len != 0) len--;
	  }
	else
	  {
	     if (ch & 0x80)
	       {
		  neew = SLSMG_BUILD_CHAR('~',color);
		  len += 1;
		  if (len > start_len)
		    {
		       if (*p != neew)
			 {
			    *p = neew;
			    flags |= TOUCHED;
			 }
		       p++;
		       if (len == max_len) break;
		       ch &= 0x7F;
		    }
	       }

	     len += 1;
	     if (len > start_len)
	       {
		  neew = SLSMG_BUILD_CHAR('^',color);
		  if (*p != neew)
		    {
		       *p = neew;
		       flags |= TOUCHED;
		    }
		  p++;
		  if (len == max_len) break;
	       }

	     if (ch == 127) ch = '?'; else ch = ch + '@';
	     len++;
	     if (len > start_len)
	       {
		  neew = SLSMG_BUILD_CHAR(ch,color);
		  if (*p != neew)
		    {
		       *p = neew;
		       flags |= TOUCHED;
		    }
		  p++;
	       }
	  }
     }

   SL_Screen[This_Row - Start_Row].flags = flags;
   This_Col = len;

   if (SLsmg_Newline_Behavior == 0)
     return;

   if (newline_flag == 0)
     {
	while (str < str_max)
	  {
	     if (*str == '\n') break;
	     str++;
	  }
	if (str == str_max) return;
	str++;
     }

   This_Row++;
   This_Col = 0;
   if (This_Row == Start_Row + Screen_Rows)
     {
	if (SLsmg_Newline_Behavior == SLSMG_NEWLINE_SCROLLS) scroll_up ();
     }
   goto top;
}

void SLsmg_write_char (char ch)
{
   SLsmg_write_nchars (&ch, 1);
}

static int Cls_Flag;

void SLsmg_cls (void)
{
   int tac;
   if (Smg_Inited == 0) return;

   tac = This_Alt_Char; This_Alt_Char = 0;
   SLsmg_set_color (0);
   clear_region (0, Screen_Rows);
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
static unsigned long compute_hash (SLsmg_Char_Type *s, int n)
{
   register unsigned long h = 0, g;
   register unsigned long sum = 0;
   register SLsmg_Char_Type *smax, ch;
   int is_blank = 2;

   s += SLsmg_Scroll_Hash_Border;
   smax = s + (n - SLsmg_Scroll_Hash_Border);
   while (s < smax)
     {
	ch = *s++;
	if (is_blank && (SLSMG_EXTRACT_CHAR(ch) != 32)) is_blank--;

	sum += ch;

	h = sum + (h << 3);
	if ((g = h & 0xE0000000UL) != 0)
	  {
	     h = h ^ (g >> 24);
	     h = h ^ g;
	  }
     }
   if (is_blank) return 0;
   return h;
}

static unsigned long Blank_Hash;

static int try_scroll_down (int rmin, int rmax)
{
   int i, r1, r2, di, j;
   unsigned long hash;
   int did_scroll;
   int color;
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
	     blank_line (SL_Screen[r1].old, Screen_Cols, ' ');
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
   int color;
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
   int i;

   bce = Bce_Color_Offset;
   Bce_Color_Offset = _SLtt_get_bce_color_offset ();
   if (bce == Bce_Color_Offset)
     return;
   
  if ((tt_Use_Blink_For_ACS != NULL)
       && (*tt_Use_Blink_For_ACS != 0))
     return;			       /* this mode does not support non-BCE
					* terminals.
					*/

   for (i = 0; i < Screen_Rows; i++)
     {
	SLsmg_Char_Type *s, *smax;

	SL_Screen[i].flags |= TRASHED;
	s = SL_Screen[i].neew;
	smax = s + Screen_Cols;
	
	while (s < smax)
	  {
	     int color = (int) SLSMG_EXTRACT_COLOR(*s);
	     int acs;

	     if (color < 0)
	       {
		  s++;
		  continue;
	       }
	     
	     acs = color & 0x80;
	     color = (color & 0x7F) - bce;
	     color += Bce_Color_Offset;
	     if (color >= 0)
	       {
		  unsigned char ch = SLSMG_EXTRACT_CHAR(*s);
		  *s = SLSMG_BUILD_CHAR(ch, ((color&0x7F)|acs));
	       }
	     s++;
	  }
     }
}
#endif

void SLsmg_refresh (void)
{
   int i;
#ifndef IBMPC_SYSTEM
   int trashed = 0;
#endif

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
	     int color = This_Color;

	     if (Cls_Flag == 0) 
	       {
		  (*tt_goto_rc) (i, 0);
		  (*tt_del_eol) ();
	       }
	     This_Color = 0;
	     blank_line (SL_Screen[i].old, Screen_Cols, ' ');
	     This_Color = color;
	  }

	SL_Screen[i].old[Screen_Cols] = 0;
	SL_Screen[i].neew[Screen_Cols] = 0;

	(*tt_smart_puts) (SL_Screen[i].neew, SL_Screen[i].old, Screen_Cols, i);

	SLMEMCPY ((char *) SL_Screen[i].old, (char *) SL_Screen[i].neew,
		  Screen_Cols * sizeof (SLsmg_Char_Type));

	SL_Screen[i].flags = 0;
#ifndef IBMPC_SYSTEM
	SL_Screen[i].old_hash = SL_Screen[i].new_hash;
#endif
     }

   if (point_visible (1)) (*tt_goto_rc) (This_Row - Start_Row, This_Col - Start_Col);
   (*tt_flush_output) ();
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
static const char Fake_Alt_Char_Pairs [] = "a:j+k+l+m+q-t+u+v+w+x|n+`+f\\g#~o,<+>.v-^h#0#";

static void init_alt_char_set (void)
{
   int i;
   unsigned const char *p, *pmax;
   unsigned char ch;

   if (Alt_Char_Set[128] == 128) return;

   i = 32;
   memset ((char *)Alt_Char_Set, ' ', i);
   while (i <= 128)
     {
	Alt_Char_Set [i] = i;
	i++;
     }

   /* Map to VT100 */
   if (*tt_Has_Alt_Charset)
     {
	if (tt_Graphics_Char_Pairs == NULL) p = NULL;
	else p = (unsigned char *) *tt_Graphics_Char_Pairs;
	if (p == NULL) return;
     }
   else	p = (unsigned char *) Fake_Alt_Char_Pairs;
   pmax = p + strlen ((char *) p);

   /* Some systems have messed up entries for this */
   while (p < pmax)
     {
	ch = *p++;
	ch &= 0x7F;		       /* should be unnecessary */
	Alt_Char_Set [ch] = *p;
	p++;
     }
}
#endif

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
   int i;
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
   int i, len;
   SLsmg_Char_Type *old, *neew;

   Smg_Inited = 0;

#ifdef REQUIRES_NON_BCE_SUPPORT
   Bce_Color_Offset = _SLtt_get_bce_color_offset ();
#endif

   Screen_Rows = *tt_Screen_Rows;
   if (Screen_Rows > MAX_SCREEN_SIZE)
     Screen_Rows = MAX_SCREEN_SIZE;

   Screen_Cols = *tt_Screen_Cols;

   This_Col = This_Row = Start_Col = Start_Row = 0;

   This_Alt_Char = 0;
   SLsmg_set_color (0);
   Cls_Flag = 1;
#ifndef IBMPC_SYSTEM
   init_alt_char_set ();
#endif
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
   
   _SLtt_color_changed_hook = SLsmg_touch_screen;
   Screen_Trashed = 1;
   Smg_Inited = 1;
   return 0;
}


int SLsmg_init_smg (void)
{
   int ret;

   BLOCK_SIGNALS;

   if (Smg_Inited)
     SLsmg_reset_smg ();

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

SLsmg_Char_Type SLsmg_char_at (void)
{
   if (Smg_Inited == 0) return 0;

   if (point_visible (1))
     {
	return SL_Screen[This_Row - Start_Row].neew[This_Col - Start_Col];
     }
   return 0;
}

void SLsmg_vprintf (const char *fmt, va_list ap)
{
   char buf[1024];

   if (Smg_Inited == 0) return;

   (void) _SLvsnprintf (buf, sizeof (buf), fmt, ap);
   SLsmg_write_string (buf);
}

void SLsmg_printf (const char *fmt, ...)
{
   va_list ap;
   unsigned int len;
   char *f;

   if (Smg_Inited == 0) return;

   va_start(ap, fmt);

   f = fmt;
   while (*f && (*f != '%'))
     f++;
   len = (unsigned int) (f - fmt);
   if (len) SLsmg_write_nchars (fmt, len);

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

void SLsmg_draw_object (int r, int c, unsigned char object)
{
   This_Row = r;  This_Col = c;

   if (Smg_Inited == 0) return;

   if (point_visible (1))
     {
	int color = This_Color;
	This_Color |= ALT_CHAR_FLAG;
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

   if ((This_Row < Start_Row) || (This_Row >= Start_Row + Screen_Rows)
       || (0 == compute_clip (This_Col, n, Start_Col, Start_Col + Screen_Cols,
			      &cmin, &cmax)))
     {
	This_Col = final_col;
	return;
     }

   if (hbuf[0] == 0)
     {
	SLMEMSET ((char *) hbuf, SLSMG_HLINE_CHAR, 16);
     }

   n = (unsigned int)(cmax - cmin);
   count = n / 16;

   save_color = This_Color;
   This_Color |= ALT_CHAR_FLAG;
   This_Col = cmin;

   SLsmg_write_nchars ((char *) hbuf, n % 16);
   while (count-- > 0)
     {
	SLsmg_write_nchars ((char *) hbuf, 16);
     }

   This_Color = save_color;
   This_Col = final_col;
}

void SLsmg_draw_vline (int n)
{
   unsigned char ch = SLSMG_VLINE_CHAR;
   int c = This_Col, rmin, rmax;
   int final_row = This_Row + n;
   int save_color;

   if (Smg_Inited == 0) return;

   if (((c < Start_Col) || (c >= Start_Col + Screen_Cols)) ||
       (0 == compute_clip (This_Row, n, Start_Row, Start_Row + Screen_Rows,
			  &rmin, &rmax)))
     {
	This_Row = final_row;
	return;
     }

   save_color = This_Color;
   This_Color |= ALT_CHAR_FLAG;

   for (This_Row = rmin; This_Row < rmax; This_Row++)
     {
	This_Col = c;
	SLsmg_write_nchars ((char *) &ch, 1);
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

void SLsmg_fill_region (int r, int c, unsigned int dr, unsigned int dc, unsigned char ch)
{
   static unsigned char hbuf[16];
   int count;
   int dcmax, rmax;

   if (Smg_Inited == 0) return;

   SLsmg_gotorc (r, c);
   r = This_Row; c = This_Col;

   dcmax = Screen_Cols - This_Col;
   if (dcmax < 0)
     return;

   if (dc > (unsigned int) dcmax) dc = (unsigned int) dcmax;

   rmax = This_Row + dr;
   if (rmax > Screen_Rows) rmax = Screen_Rows;

#if 0
   ch = Alt_Char_Set[ch];
#endif
   if (ch != hbuf[0]) SLMEMSET ((char *) hbuf, (char) ch, 16);

   for (This_Row = r; This_Row < rmax; This_Row++)
     {
	This_Col = c;
	count = dc / 16;
	SLsmg_write_nchars ((char *) hbuf, dc % 16);
	while (count-- > 0)
	  {
	     SLsmg_write_nchars ((char *) hbuf, 16);
	  }
     }

   This_Row = r;
}

void SLsmg_forward (int n)
{
   This_Col += n;
}

void SLsmg_write_color_chars (SLsmg_Char_Type *s, unsigned int len)
{
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

#if REQUIRES_NON_BCE_SUPPORT
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

void
SLsmg_set_color_in_region (int color, int r, int c, unsigned int dr, unsigned int dc)
{
   int cmax, rmax;
   SLsmg_Char_Type char_mask;

   if (Smg_Inited == 0) return;

   c -= Start_Col;
   r -= Start_Row;

   cmax = c + (int) dc;
   rmax = r + (int) dr;

   if (cmax > Screen_Cols) cmax = Screen_Cols;
   if (rmax > Screen_Rows) rmax = Screen_Rows;

   if (c < 0) c = 0;
   if (r < 0) r = 0;

#if REQUIRES_NON_BCE_SUPPORT
   if (Bce_Color_Offset)
     {
	if (color & 0x80)
	  color = ((color & 0x7F) + Bce_Color_Offset) | 0x80;
	else
	  color = ((color & 0x7F) + Bce_Color_Offset) & 0x7F;
     }
#endif
   color = color << 8;

   char_mask = 0xFF;

#ifndef IBMPC_SYSTEM
   if ((tt_Use_Blink_For_ACS == NULL)
       || (0 == *tt_Use_Blink_For_ACS))
     char_mask = 0x80FF;
#endif

   while (r < rmax)
     {
	SLsmg_Char_Type *s, *smax;

	SL_Screen[r].flags |= TOUCHED;
	s = SL_Screen[r].neew;
	smax = s + cmax;
	s += c;

	while (s < smax)
	  {
	     *s = (*s & char_mask) | color;
	     s++;
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
     SLang_exit_error ("Terminal not powerful enough for SLsmg");

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
   tt_Has_Alt_Charset = tt->tt_has_alt_charset;
   tt_Use_Blink_For_ACS = tt->tt_use_blink_for_acs;
   tt_Graphics_Char_Pairs = tt->tt_graphic_char_pairs;
#endif

   tt_Screen_Cols = tt->tt_screen_cols;
   tt_Screen_Rows = tt->tt_screen_rows;
}

