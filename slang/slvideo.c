/* -*- mode: C; mode: fold -*- */
/* Copyright (c) 1992, 1997, 2001, 2002, 2003 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

/* This file is best edited with a folding editor */

#include "slinclud.h"

#if !defined(__WIN32__) && !defined(__IBMC__)
# include <dos.h>
#endif

#include "slang.h"
#include "_slang.h"

int SLtt_Term_Cannot_Insert;
int SLtt_Term_Cannot_Scroll;
int SLtt_Ignore_Beep = 3;
int SLtt_Use_Ansi_Colors;
int SLtt_Has_Status_Line = 0;
int SLtt_Screen_Rows = 25;
int SLtt_Screen_Cols = 80;
int SLtt_Msdos_Cheap_Video = 0;

void (*_SLtt_color_changed_hook)(void);

/* This definition will need changing when SLsmg_Char_Type changes. */
#define SLSMG_CHAR_TO_USHORT(x) ((unsigned short)(x))

/*{{{ ------------- static local variables ---------- */

static int Attribute_Byte;
static int Scroll_r1 = 0, Scroll_r2 = 25;
static int Cursor_Row = 1, Cursor_Col = 1;
static int Current_Color;
static int IsColor = 1;
static int Blink_Killed = 1;	/* high intensity background enabled */

#define JMAX_COLORS	256
#define JNORMAL_COLOR	0
#define JNO_COLOR	-1

static unsigned char Color_Map [JMAX_COLORS] =
{
   0x7, 0x70, 0x70, 0x70, 0x70, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
   0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7
};

#define JMAX_COLOR_NAMES 16
static const char * const Color_Names [JMAX_COLOR_NAMES] =
{
   "black", "blue", "green", "cyan",
     "red", "magenta", "brown", "lightgray",
     "gray", "brightblue", "brightgreen", "brightcyan",
     "brightred", "brightmagenta", "yellow", "white"
};

static void fixup_colors (void);

/*}}}*/

static void goto_rc_abs (int r, int c)
{
   SLtt_goto_rc (r - Scroll_r1, c);
}

#if defined(__BORLANDC__) && defined(__MSDOS__)
# define IBMPC_ASM_VIDEO	1
#endif

#if defined(__WATCOMC__) && !defined(__NT__) && !defined(__os2__)
# define WATCOM_VIDEO		1
#endif

#if defined (__GO32__)
# define GO32_VIDEO		1
#endif

#if defined (__EMX__)		/* EMX video does both DOS & OS/2 */
# define EMX_VIDEO		1
#else
# if defined(__os2__)
#  define OS2_VIDEO		1
# endif
#endif

#if defined (__WIN32__)
# define WIN32_VIDEO		1
#endif

/* The functions in these folds contain somewhat video system specific code
 * that if merged together into single functions will become a confusing
 * mess.
 */

#ifdef IBMPC_ASM_VIDEO /*{{{*/

# include <conio.h>
# include <bios.h>
# include <mem.h>

/* buffer to hold a line of character/attribute pairs */
#define MAXCOLS 256
static unsigned char Line_Buffer [MAXCOLS*2];

#define MK_SPACE_CHAR()	(((Attribute_Byte) << 8) | 0x20)

static unsigned char *Video_Base;
# define MK_SCREEN_POINTER(row,col)	((unsigned short *)\
					 (Video_Base +\
					  2 * (SLtt_Screen_Cols * (row)\
					       + (col))))
static int Video_Status_Port;

# define MONO_STATUS	0x3BA
# define CGA_STATUS	0x3DA
# define CGA_SETMODE	0x3D8

# define SNOW_CHECK \
  if (SLtt_Msdos_Cheap_Video)\
    { while ((inp (CGA_STATUS) & 0x08)); while (!(inp (CGA_STATUS) & 0x08)); }

void SLtt_write_string (char *str)
{
   /* FIXME: Priority=medium
    * This should not go to stdout. */
   fputs (str, stdout);
}

/* row is with respect to the scrolling region. */
void SLtt_goto_rc (int r, int c)
{
   union REGS regs;

   r += Scroll_r1;

   if (r > SLtt_Screen_Rows - 1) r = SLtt_Screen_Rows - 1;
   if (c > SLtt_Screen_Cols - 1) c = SLtt_Screen_Cols - 1;

   Cursor_Row = r;
   Cursor_Col = c;

   regs.h.dh = r;
   regs.h.dl = c;
   regs.h.bh = 0;
   regs.h.ah = 2;
   int86 (0x10, &regs, &regs);
}

static void asm_video_getxy (void)
{
   asm  mov ah, 3
     asm  mov bh, 0
     asm  int 10h
     asm  xor ax, ax
     asm  mov al, dh
     asm  mov Cursor_Row, ax
     asm  xor ax, ax
     asm  mov al, dl
     asm  mov Cursor_Col, ax
}

void SLtt_begin_insert (void)
{
   unsigned short *p;
   int n;

   asm_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col;
   p = MK_SCREEN_POINTER (Cursor_Row, SLtt_Screen_Cols - 1);

   SNOW_CHECK;
   asm  mov ax, ds
     asm  mov bx, di
     asm  mov dx, si

     asm  mov cx, n
     asm  les di, p
     asm  lds si, p
     asm  sub si, 2
     asm  std
     asm  rep movsw

     asm  mov ds, ax
     asm  mov di, bx
     asm  mov si, dx
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
   unsigned short *p;
   int n;

   asm_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col - 1;
   p = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);

   SNOW_CHECK;
   asm  mov ax, ds
     asm  mov bx, si
     asm  mov dx, di

     asm  mov cx, n
     asm  les di, p
     asm  lds si, p
     asm  add si, 2
     asm  cld
     asm  rep movsw

     asm  mov ds, ax
     asm  mov si, bx
     asm  mov di, dx
}

void SLtt_erase_line (void)
{
   unsigned short w, *p;

   p = MK_SCREEN_POINTER (Cursor_Row, 0);
   Attribute_Byte = 0x07;

   w = MK_SPACE_CHAR ();

   SNOW_CHECK;
   asm  mov dx, di
     asm  mov ax, w
     asm  mov cx, SLtt_Screen_Cols
     asm  les di, p
     asm  cld
     asm  rep stosw
     asm  mov di, dx

     Current_Color = JNO_COLOR;		/* since we messed with attribute byte */
}

void SLtt_delete_nlines (int nlines)
{
   SLtt_normal_video ();

   /* This has the effect of pulling all lines below it up */
   asm  mov ax, nlines
     asm  mov ah, 6		/* int 6h */
     asm  xor cx, cx
     asm  mov ch, byte ptr Scroll_r1
     asm  mov dx, SLtt_Screen_Cols
     asm  dec dx
     asm  mov dh, byte ptr Scroll_r2
     asm  mov bh, byte ptr Attribute_Byte
     asm  int 10h
}

void SLtt_reverse_index (int nlines)
{
   SLtt_normal_video ();
   asm  xor cx, cx
     asm  mov ch, byte ptr Scroll_r1
     asm  mov dx, SLtt_Screen_Cols
     asm  dec dx
     asm  mov dh, byte ptr Scroll_r2
     asm  mov bh, byte ptr Attribute_Byte
     asm  mov ah, 7
     asm  mov al, byte ptr nlines
     asm  int 10h
}

static void asm_video_invert_region (int top_row, int bot_row)
{
   register unsigned short ch, sh;
   register unsigned short *pmin = MK_SCREEN_POINTER (top_row, 0);
   register unsigned short *pmax = MK_SCREEN_POINTER (bot_row, 0);

   while (pmin < pmax)
     {
	sh = *pmin;
	ch = sh;
	ch = ch ^ 0xFF00;
	*pmin = (ch & 0xFF00) | (sh & 0x00FF);
	pmin++;
     }
}

void SLtt_del_eol (void)
{
   unsigned short *p;
   unsigned short w;
   int n;

   n = SLtt_Screen_Cols - Cursor_Col;
   p = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);
   if (Current_Color != JNO_COLOR) SLtt_normal_video ();
   w = MK_SPACE_CHAR ();

   SNOW_CHECK;
   asm  mov dx, di
     asm  les di, p
     asm  mov ax, w
     asm  mov cx, n
     asm  cld
     asm  rep stosw

     asm  mov di, dx
}

static unsigned short *asm_video_write (register unsigned char *pp,
					register unsigned char *p,
					register unsigned short *pos)
{
   int n = (int) (p - pp);	/* num of characters of PP to write */

   asm  push si
     asm  push ds
     asm  push di

   /* set up register for BOTH fast and slow */
     asm  mov bx, SLtt_Msdos_Cheap_Video

   /* These are the registers needed for both fast AND slow */
     asm  mov ah, byte ptr Attribute_Byte
     asm  mov cx, n
     asm  lds si, dword ptr pp
     asm  les di, dword ptr pos
     asm  cld

     asm  cmp bx, 0		       /* cheap video test */
     asm  je L_fast
     asm  mov bx, ax
     asm  mov dx, CGA_STATUS
     asm  jg L_slow_blank

   /* slow video */
     asm  cli

   /* wait for retrace */
     L_slow:
   asm  in al, dx
     asm  test al, 1
     asm  jnz L_slow

     L_slow1:
   asm  in al, dx
     asm  test al, 1
     asm  jz L_slow1

   /* move a character out */
     asm  mov ah, bh
     asm  lodsb
     asm  stosw
     asm  loop L_slow

     asm  sti
     asm  jmp done

/* -------------- slow video, vertical retace and pump --------------*/
     L_slow_blank:
   L_slow_blank_loop:
   asm  in al, dx
     asm  test al, 8
     asm  jnz L_slow_blank_loop

     L_slow_blank1:
   asm  in al, dx
     asm  test al, 8
     asm  jz L_slow_blank1
   /* write line */
     asm  mov ah, bh
     L_slow_blank2:
   asm  lodsb
     asm  stosw
     asm  loop L_slow_blank2

     asm jmp done
/*-------------- Fast video --------------*/

     L_fast:
   asm  lodsb
     asm  stosw
     asm  loop L_fast
     done:
   asm  pop di
     asm  pop ds
     asm  pop si
     return (pos + n);
}

static void write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   register unsigned char *p;
   register unsigned short pair;
   unsigned char ch, color;
   register unsigned short *pos;

   p = Line_Buffer;
   pos = MK_SCREEN_POINTER (Cursor_Row, 0);

   while (count--)
     {
	pair = SLSMG_CHAR_TO_USHORT(*src);	/* character/color pair */
	src++;
	ch = pair & 0xff;	/* character value */
	color = pair >> 8;	/* color value */
	if (color != Current_Color)	/* need a new color */
	  {
	     if (p != Line_Buffer)
	       {
		  pos = asm_video_write (Line_Buffer, p, pos);
		  p = Line_Buffer;
	       }
	     SLtt_reverse_video (color);	/* change color */
	  }
	*(p++) = ch;
     }
   pos = asm_video_write (Line_Buffer, p, pos);
}

void SLtt_cls (void)
{
   SLtt_normal_video ();

   asm  mov dx, SLtt_Screen_Cols
     asm  dec dx
     asm  mov ax, SLtt_Screen_Rows
     asm  dec ax
     asm  mov dh, al
     asm  xor cx, cx
     asm  xor ax, ax
     asm  mov ah, 7
     asm  mov bh, byte ptr Attribute_Byte
     asm  int 10h
}

void SLtt_putchar (char ch)
{
   unsigned short p, *pp;

   if (Current_Color) SLtt_normal_video ();
   asm_video_getxy ();		/* get current position */
   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:
	/* write character to screen */
	pp = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);
	p = (Attribute_Byte << 8) | (unsigned char) ch;
	SNOW_CHECK;
	*pp = p;
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_screen_size (void)
{
   int w, h;

   h = 0;

   /* Get BIOS's screenwidth, this works on ALL displays. */
   w = *((int *)MK_FP(0x40, 0x4a));

   /* Use Ralf Brown test for EGA or greater */
   asm mov ah, 12h
   asm mov bl, 10h
   asm mov bh, 0xFF  /* EGA or greater will change this */
   asm int 10h
   asm cmp bh, 0xFF
   asm je L1
     /* if EGA or compatible: Get BIOS's number of rows. */
     h = *(char *)MK_FP(0x40, 0x84) + 1;
   /* scan_lines = *(int *) 0x485; */

   L1:
      if (h <= 0) h = 25;

   SLtt_Screen_Rows = h;
   SLtt_Screen_Cols = w;
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

/*----------------------------------------------------------------------*\
 * Function:	int SLtt_init_video (void);
\*----------------------------------------------------------------------*/
int SLtt_init_video (void)
{
   unsigned char *p;

#ifdef HAS_SAVE_SCREEN
   save_screen ();
#endif

   Cursor_Row = Cursor_Col = 0;
   p = (unsigned char far *) 0x00400049L;
   if (*p == 7)
     {
	Video_Status_Port = MONO_STATUS;
	Video_Base = (unsigned char *) MK_FP (0xb000,0000);
	IsColor = 0;
     }
   else
     {
	Video_Status_Port = CGA_STATUS;
	Video_Base = (unsigned char *) MK_FP (0xb800,0000);
	IsColor = 1;
     }

   /* test for video adapter type.  Of primary interest is whether there is
    * snow or not.  Assume snow if the card is color and not EGA or greater.
    */

   /* Use Ralf Brown test for EGA or greater */
   asm  mov ah, 0x12
     asm  mov bl, 0x10
     asm  mov bh, 0xFF
     asm  int 10h
     asm  cmp bh, 0xFF
     asm  je L1

   /* (V)EGA */
     asm  xor bx, bx
     asm  mov SLtt_Msdos_Cheap_Video, bx
     asm  mov ax, Attribute_Byte
     asm  cmp ax, bx
     asm  jne L2
     asm  mov ax, 0x17
     asm  mov Attribute_Byte, ax
     asm  jmp L2

     L1:
   /* Not (V)EGA */
   asm  mov ah, 0x0F
     asm  int 10h
     asm  cmp al, 7
     asm  je L3
     asm  mov ax, 1
     asm  mov SLtt_Msdos_Cheap_Video, ax
     L3:
   asm  mov ax, Attribute_Byte
     asm  cmp ax, 0
     asm  jne L2
     asm  mov ax, 0x07
     asm  mov Attribute_Byte, ax
     L2:
   /* toggle the blink bit so we can use hi intensity background */
   if (IsColor && !SLtt_Msdos_Cheap_Video)
     {
	asm  mov ax, 0x1003
	  asm  mov bx, 0
	  asm  int 0x10
	  Blink_Killed = 1;
     }

   SLtt_Use_Ansi_Colors = IsColor;
   SLtt_get_screen_size ();
   SLtt_reset_scroll_region ();
   fixup_colors ();
   return 0;
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);
   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) asm_video_invert_region (special, visual);
   if (audible) sound (1500); delay (100); if (audible) nosound ();
   if (visual) asm_video_invert_region (special, visual);
}

#endif				       /* IBMPC_ASM_VIDEO */

/*}}}*/

#ifdef GO32_VIDEO /*{{{*/

# include <pc.h>
# define HAS_SAVE_SCREEN	1

# ifdef HAS_SAVE_SCREEN
static void *Saved_Screen_Buffer;
static int Saved_Cursor_Row;

static void save_screen (void)
{
   int row, col;

   SLfree ((char *) Saved_Screen_Buffer);
   Saved_Screen_Buffer = NULL;

   Saved_Screen_Buffer = (short *) SLmalloc (sizeof (short) *
				   ScreenCols () * ScreenRows ());

   if (Saved_Screen_Buffer == NULL)
     return;

   ScreenRetrieve (Saved_Screen_Buffer);
   ScreenGetCursor (&row, &col);
   Saved_Cursor_Row = row;
}

static void restore_screen (void)
{
   if (Saved_Screen_Buffer == NULL) return;
   ScreenUpdate (Saved_Screen_Buffer);
   goto_rc_abs (Saved_Cursor_Row, 0);
}
#endif				       /* HAS_SAVE_SCREEN */

void SLtt_write_string (char *str)
{
   while (Cursor_Col < SLtt_Screen_Cols)
     {
	char ch = *str++;

	if (ch == 0)
	  break;

	ScreenPutChar (ch, Attribute_Byte, Cursor_Col, Cursor_Row);
	Cursor_Col++;
     }
   goto_rc_abs (Cursor_Row, Cursor_Col);
}

void SLtt_goto_rc (int row, int col)
{
   row += Scroll_r1;
   if (row > SLtt_Screen_Rows) row = SLtt_Screen_Rows;
   if (col > SLtt_Screen_Cols) col = SLtt_Screen_Cols;

   ScreenSetCursor (row, col);

   Cursor_Row = row;
   Cursor_Col = col;
}

static void go32_video_getxy (void)
{
   ScreenGetCursor (&Cursor_Row, &Cursor_Col);
}

static void go32_video_deleol (int x)
{
   while (x < SLtt_Screen_Cols)
     ScreenPutChar (32, Attribute_Byte, x++, Cursor_Row);
}

void SLtt_begin_insert (void)
{
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
}

void SLtt_erase_line (void)
{
   Attribute_Byte = 0x07;
   go32_video_deleol (0);
   Current_Color = JNO_COLOR;		/* since we messed with attribute byte */
}

void SLtt_delete_nlines (int nlines)
{
   union REGS r;

   SLtt_normal_video ();

   r.x.ax = nlines;
   r.x.cx = 0;
   r.h.ah = 6;
   r.h.ch = Scroll_r1;
   r.h.dl = SLtt_Screen_Cols - 1;
   r.h.dh = Scroll_r2;
   r.h.bh = Attribute_Byte;
   int86 (0x10, &r, &r);
}

void SLtt_reverse_index (int nlines)
{
   union REGS r;

   SLtt_normal_video ();

   r.h.al = nlines;
   r.x.cx = 0;
   r.h.ah = 7;
   r.h.ch = Scroll_r1;
   r.h.dl = SLtt_Screen_Cols - 1;
   r.h.dh = Scroll_r2;
   r.h.bh = Attribute_Byte;
   int86 (0x10, &r, &r);
}

static void go32_video_invert_region (int top_row, int bot_row)
{
   unsigned char buf [2 * 180 * 80];         /* 180 cols x 80 rows */
   unsigned char *b, *bmax;

   b    = buf + 1 + 2 * SLtt_Screen_Cols * top_row;
   bmax = buf + 1 + 2 * SLtt_Screen_Cols * bot_row;

   ScreenRetrieve (buf);
   while (b < bmax)
     {
	*b ^= 0xFF;
	b += 2;
     }
   ScreenUpdate (buf);
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);
   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) go32_video_invert_region (special, visual);
   if (audible) sound (1500); delay (100); if (audible) nosound ();
   if (visual) go32_video_invert_region (special, visual);
}

void SLtt_del_eol (void)
{
   if (Current_Color != JNO_COLOR) SLtt_normal_video ();
   go32_video_deleol (Cursor_Col);
}

static void
write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   register unsigned short pair;
   unsigned int n;

   /* write into a character/attribute pair */
   n = Cursor_Col;
   while (count)
     {
	pair = SLSMG_CHAR_TO_USHORT(*src);/* character/color pair */
	src++;
	SLtt_reverse_video (pair >> 8);	/* color change */
	ScreenPutChar ((int)pair & 0xFF, Attribute_Byte, n, Cursor_Row);
	n++;
	count--;
     }
}

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_cls (void);
\*----------------------------------------------------------------------*/
void SLtt_cls (void)
{
   SLtt_normal_video ();
   SLtt_reset_scroll_region ();
   SLtt_goto_rc (0, 0);
   SLtt_delete_nlines (SLtt_Screen_Rows);
}

void SLtt_putchar (char ch)
{
   if (Current_Color) SLtt_normal_video ();

   go32_video_getxy ();		/* get current position */

   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:			/* write character to screen */
	ScreenPutChar ((int) ch, Attribute_Byte, Cursor_Col, Cursor_Row);
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_screen_size (void)
{
   SLtt_Screen_Rows = ScreenRows ();
   SLtt_Screen_Cols = ScreenCols ();
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

int SLtt_init_video (void)
{
#ifdef HAS_SAVE_SCREEN
   save_screen ();
#endif

   if (!Attribute_Byte) Attribute_Byte = 0x17;

   IsColor = 1;			/* is it really? */

   if (IsColor)
     {
	union REGS r;
	r.x.ax = 0x1003; r.x.bx = 0;
	int86 (0x10, &r, &r);
	Blink_Killed = 1;
     }

   Cursor_Row = Cursor_Col = 0;

   SLtt_Term_Cannot_Insert = 1;
   SLtt_reset_scroll_region ();
   SLtt_Use_Ansi_Colors = IsColor;
   fixup_colors ();
   return 0;
}

#endif				       /* GO32_VIDEO */

/*}}}*/

#ifdef EMX_VIDEO /*{{{*/

# define INCL_VIO
# define INCL_DOSPROCESS
# include <os2.h>
# include <os2emx.h>
# include <sys/video.h>

static VIOMODEINFO vioModeInfo;
/* buffer to hold a line of character/attribute pairs */
#define MAXCOLS 256
static unsigned char Line_Buffer [MAXCOLS*2];

/* this is how to make a space character */
#define MK_SPACE_CHAR()	(((Attribute_Byte) << 8) | 0x20)

void SLtt_write_string (char *str)
{
   /* FIXME: Priority=medium
    * This should not go to stdout. */
   fputs (str, stdout);
}

void SLtt_goto_rc (int row, int col)
{
   row += Scroll_r1;
   if (row > SLtt_Screen_Rows) row = SLtt_Screen_Rows;
   if (col > SLtt_Screen_Cols) col = SLtt_Screen_Cols;
   v_gotoxy (col, row);
   Cursor_Row = row;
   Cursor_Col = col;
}

static void emx_video_getxy (void)
{
   v_getxy (&Cursor_Col, &Cursor_Row);
}

static void emx_video_deleol (int x)
{
   unsigned char *p, *pmax;
   int count = SLtt_Screen_Cols - x;
   int w = MK_SPACE_CHAR ();

   p = Line_Buffer;
   pmax = p + 2 * count;

   while (p < pmax)
     {
	*p++ = (unsigned char) w;
	*p++ = (unsigned char) (w >> 8);
     }

   v_putline (Line_Buffer, x, Cursor_Row, count);
}

void SLtt_begin_insert (void)
{
   int n;

   emx_video_getxy ();

   n = SLtt_Screen_Cols - Cursor_Col;
   v_getline (Line_Buffer, Cursor_Col, Cursor_Row, n);
   v_putline (Line_Buffer, Cursor_Col+1, Cursor_Row, n - 1);
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
   int n;

   emx_video_getxy ();

   n = SLtt_Screen_Cols - Cursor_Col - 1;
   v_getline (Line_Buffer, Cursor_Col+1, Cursor_Row, n);
   v_putline (Line_Buffer, Cursor_Col, Cursor_Row, n);
}

void SLtt_erase_line (void)
{
   Attribute_Byte = 0x07;
   emx_video_deleol (0);
   Current_Color = JNO_COLOR;		/* since we messed with attribute byte */
}

void SLtt_delete_nlines (int nlines)
{
   SLtt_normal_video ();
   v_attrib (Attribute_Byte);
   v_scroll (0, Scroll_r1, SLtt_Screen_Cols-1, Scroll_r2, nlines, V_SCROLL_UP);
}

void SLtt_reverse_index (int nlines)
{
   SLtt_normal_video ();

   v_attrib (Attribute_Byte);
   v_scroll (0, Scroll_r1, SLtt_Screen_Cols-1, Scroll_r2, nlines,
	     V_SCROLL_DOWN);
}

static void emx_video_invert_region (int top_row, int bot_row)
{
   int row, col;

   for (row = top_row; row < bot_row; row++)
     {
	v_getline (Line_Buffer, 0, row, SLtt_Screen_Cols);
	for (col = 1; col < SLtt_Screen_Cols * 2; col += 2)
	  Line_Buffer [col] ^= 0xff;
	v_putline (Line_Buffer, 0, row, SLtt_Screen_Cols);
     }
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);
   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) emx_video_invert_region (special, visual);
   if (audible) /*sound (1500)*/; _sleep2 (100); if (audible) /* nosound () */;
   if (visual) emx_video_invert_region (special, visual);
}

void SLtt_del_eol (void)
{
   if (Current_Color != JNO_COLOR) SLtt_normal_video ();
   emx_video_deleol (Cursor_Col);
}

static void
write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   register unsigned char *p = Line_Buffer;
   register unsigned short pair;
   int n = count;

   /* write into a character/attribute pair */
   while (n-- > 0)
     {
	pair = SLSMG_CHAR_TO_USHORT(*src);/* character/color pair */
	src++;
	SLtt_reverse_video (pair >> 8);	/* color change */
	*(p++) = pair & 0xff;		/* character byte */
	*(p++) = Attribute_Byte;	/* attribute byte */
     }
   v_putline (Line_Buffer, Cursor_Col, Cursor_Row, count);
}

void SLtt_cls (void)
{
   SLtt_normal_video ();
   SLtt_reset_scroll_region ();
   SLtt_goto_rc (0, 0);
   SLtt_delete_nlines (SLtt_Screen_Rows);
}

void SLtt_putchar (char ch)
{
   if (Current_Color) SLtt_normal_video ();

   emx_video_getxy ();		/* get current position */
   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:			/* write character to screen */
	v_putn (ch, 1);
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

void SLtt_get_screen_size (void)
{
   vioModeInfo.cb = sizeof(vioModeInfo);
   VioGetMode (&vioModeInfo, 0);
   SLtt_Screen_Cols = vioModeInfo.col;
   SLtt_Screen_Rows = vioModeInfo.row;
}

int SLtt_init_video (void)
{
   int OldCol, OldRow;
   PTIB   ptib;
   PPIB   ppib;
   USHORT args[3] = { 6, 2, 1 };

#ifdef HAS_SAVE_SCREEN
   save_screen ();
#endif

   Cursor_Row = Cursor_Col = 0;

   v_init ();
   if ( v_hardware () != V_MONOCHROME ) IsColor = 1; else IsColor = 0;

   v_getxy(&OldCol,&OldRow);
   v_gotoxy (0, 0);
   if (IsColor)
     {
	if (_osmode == OS2_MODE)
	  {
# if 0
	     /* Enable high-intensity background colors */
	     VIOINTENSITY RequestBlock;
	     RequestBlock.cb = sizeof (RequestBlock);
	     RequestBlock.type = 2; RequestBlock.fs = 1;
	     VioSetState (&RequestBlock, 0);	/* nop if !fullscreen */
# endif
	  }
     }

   DosGetInfoBlocks (&ptib, &ppib);
   if ((ppib->pib_ultype) == 2)        /* VIO */
     Blink_Killed = 1;
   else
     {                              /* Fullscreen */
        if (VioSetState (args, 0) == 0)
	  Blink_Killed = 1;
        else
	  Blink_Killed = 0;
     }

   if (!Attribute_Byte)
     {
	/* find the attribute currently under the cursor */
	v_getline (Line_Buffer, OldCol, OldRow, 1);
	Attribute_Byte = Line_Buffer[1];
	SLtt_set_color (JNORMAL_COLOR, NULL,
			Color_Names[(Attribute_Byte) & 0xf],
			Color_Names[(Attribute_Byte) >> 4]);
     }

   v_attrib (Attribute_Byte);

   fixup_colors ();

   SLtt_get_screen_size ();
   SLtt_Use_Ansi_Colors = IsColor;
   SLtt_reset_scroll_region ();
   return 0;
}

#endif				       /* EMX_VIDEO */

/*}}}*/

#ifdef WIN32_VIDEO /*{{{*/

#include <windows.h>

static HANDLE hStdout = INVALID_HANDLE_VALUE;

#define MAXCOLS 256
static CHAR_INFO Line_Buffer [MAXCOLS];

void SLtt_write_string (char *str)
{
   DWORD bytes;
   int n, c;
   if (str == NULL) return;

   n = (int) strlen (str);
   c = n + Cursor_Col;
   if (c >= SLtt_Screen_Cols)
     n = SLtt_Screen_Cols - Cursor_Col;
   if (n < 0) n = 0;

   (void) WriteConsole (hStdout, str, (unsigned int) n, &bytes, NULL);

   goto_rc_abs (Cursor_Row, Cursor_Col + n);
}

void SLtt_goto_rc (int row, int col)
{
   COORD newPosition;

   row += Scroll_r1;
   if (row > SLtt_Screen_Rows) row = SLtt_Screen_Rows;
   if (col > SLtt_Screen_Cols) col = SLtt_Screen_Cols;
   newPosition.X = col;
   newPosition.Y = row;

   (void) SetConsoleCursorPosition(hStdout, newPosition);

   Cursor_Row = row;
   Cursor_Col = col;
}

static void win32_video_getxy (void)
{
   CONSOLE_SCREEN_BUFFER_INFO screenInfo;

   if (TRUE == GetConsoleScreenBufferInfo(hStdout, &screenInfo))
     {
	Cursor_Row = screenInfo.dwCursorPosition.Y;
	Cursor_Col = screenInfo.dwCursorPosition.X;
     }
}

static void win32_video_hscroll (int n)
{
   SMALL_RECT rc;
   COORD c;
   CHAR_INFO ci;
   WORD w = 227;
   DWORD d;

   win32_video_getxy ();

   rc.Left = Cursor_Col;
   rc.Right = SLtt_Screen_Cols;
   rc.Top = rc.Bottom = Cursor_Row;

   c.Y = Cursor_Row;
#if 1
   c.X = SLtt_Screen_Cols - 1;
   ReadConsoleOutputAttribute(hStdout, &w, 1, c, &d);
#else
   /* New region gets the current color */
   w = Attribute_Byte;
#endif
   c.X = Cursor_Col + n;

   ci.Char.AsciiChar = ' ';
   ci.Attributes = w;

   ScrollConsoleScreenBuffer(hStdout, &rc, &rc, c, &ci);
}

static void win32_video_deleol (int x)
{
   DWORD d;
   COORD c;

   c.X = x;
   c.Y = Cursor_Row;

   x = SLtt_Screen_Cols - x;
   FillConsoleOutputCharacter(hStdout, ' ', x, c, &d);
   FillConsoleOutputAttribute(hStdout, (char)Attribute_Byte, x, c, &d);
}

static void win32_video_vscroll (int n)
{
   SMALL_RECT rc, clip_rc;
   COORD c;
   CHAR_INFO ci;

   SLtt_normal_video();

   /* ScrollConsoleScreenBuffer appears to have a bug when
    * Scroll_r1 == Scroll_r2.  Sigh.
    */
   if (Scroll_r2 == Scroll_r1)
     {
	SLtt_goto_rc (0, 0);
	win32_video_deleol (0);
	return;
     }

   rc.Left = clip_rc.Left = 0;
   rc.Right = clip_rc.Right = SLtt_Screen_Cols - 1;
   rc.Top = clip_rc.Top = Scroll_r1;
   rc.Bottom = clip_rc.Bottom = Scroll_r2;

   c.X = 0;
   c.Y = Scroll_r1 + n;

   ci.Char.AsciiChar = ' ';
   ci.Attributes = Attribute_Byte;

   ScrollConsoleScreenBuffer(hStdout, &rc, &clip_rc, c, &ci);
}

void SLtt_begin_insert (void)
{
   win32_video_hscroll (1);
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
   win32_video_hscroll (-1);
}

void SLtt_erase_line (void)
{
   Attribute_Byte = 0x7;
   win32_video_deleol (0);
   Current_Color = JNO_COLOR;
}

void SLtt_delete_nlines (int nlines)
{
   win32_video_vscroll (-nlines);
}

void SLtt_reverse_index (int nlines)
{
   win32_video_vscroll (nlines);
}

static void win32_invert_region (int top_row, int bot_row)
{
   (void) top_row; (void) bot_row;
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);

   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) win32_invert_region (special, visual);
   if (audible) Beep (1500, 100); else Sleep (100);
   if (visual) win32_invert_region (special, visual);
}

void SLtt_del_eol (void)
{
   if (Current_Color != JNO_COLOR)
     SLtt_normal_video ();
   win32_video_deleol (Cursor_Col);
}

static void
write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   unsigned short pair;
   COORD coord, c;
   CHAR_INFO *p;
   unsigned int n;
   SMALL_RECT rc;

   /* write into a character/attribute pair */
   n = count;
   p = Line_Buffer;
   while (n)
     {
	n--;
	pair = SLSMG_CHAR_TO_USHORT(*src);/* character/color pair */
	src++;
	SLtt_reverse_video (pair >> 8);	/* color change */
	p->Char.AsciiChar = pair & 0xff;
	p->Attributes = Attribute_Byte;
	p++;
     }

   c.X = count;
   c.Y = 1;
   coord.X = coord.Y = 0;
   rc.Left = Cursor_Col;
   rc.Right = Cursor_Col + count - 1;
   rc.Top = rc.Bottom = Cursor_Row;
   WriteConsoleOutput(hStdout, Line_Buffer, c, coord, &rc);
}

void SLtt_cls (void)
{
   DWORD bytes;
   COORD coord;
   char ch;

   SLtt_normal_video ();
   /* clear the WIN32 screen in one shot */
   coord.X = 0;
   coord.Y = 0;

   ch = ' ';

   (void) FillConsoleOutputCharacter(hStdout,
				     ch,
				     SLtt_Screen_Cols * SLtt_Screen_Rows,
				     coord,
				     &bytes);

     /* now set screen to the current attribute */
   ch = Attribute_Byte;
   (void) FillConsoleOutputAttribute(hStdout,
				     ch,
				     SLtt_Screen_Cols * SLtt_Screen_Rows,
				     coord,
				     &bytes);
}

void SLtt_putchar (char ch)
{
   DWORD bytes;
   WORD attr;
   COORD c;

   if (Current_Color) SLtt_normal_video ();
   win32_video_getxy ();
   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:			/* write character to screen */
	c.X = Cursor_Col;
	c.Y = Cursor_Row;
	attr = Attribute_Byte;
	WriteConsoleOutputCharacter(hStdout, &ch, 1, c, &bytes);
	WriteConsoleOutputAttribute(hStdout, &attr, 1, c, &bytes);
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_screen_size (void)
{
   CONSOLE_SCREEN_BUFFER_INFO csbi;
   HANDLE h;

   h = hStdout;
   if (h == INVALID_HANDLE_VALUE)
     h = GetStdHandle (STD_OUTPUT_HANDLE);

   if ((h == INVALID_HANDLE_VALUE)
       || (FALSE == GetConsoleScreenBufferInfo(h, &csbi)))
     {
	SLang_exit_error ("Unable to determine the screen size");
	return;
     }
#if 0
   SLtt_Screen_Rows = csbi.dwSize.Y;
   SLtt_Screen_Cols = csbi.dwSize.X;
#else
   SLtt_Screen_Rows = (csbi.srWindow.Bottom - csbi.srWindow.Top) + 1;
   SLtt_Screen_Cols = (csbi.srWindow.Right - csbi.srWindow.Left) + 1;
#endif
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

static int win32_resize (void)
{
   SMALL_RECT windowRect;

   SLtt_get_screen_size ();

   windowRect.Left = 0;
   windowRect.Top = 0;
   windowRect.Right = SLtt_Screen_Cols - 1;
   windowRect.Bottom = SLtt_Screen_Rows - 1;

   if (FALSE == SetConsoleWindowInfo(hStdout, TRUE, &windowRect))
     return -1;

   return 0;
}

static int win32_init (void)
{
   SECURITY_ATTRIBUTES sec;

   memset ((char *) &sec, 0, sizeof(SECURITY_ATTRIBUTES));
   sec.nLength = sizeof (SECURITY_ATTRIBUTES);
   sec.bInheritHandle = FALSE;

   hStdout = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
				       FILE_SHARE_READ|FILE_SHARE_WRITE,
				       &sec,
				       CONSOLE_TEXTMODE_BUFFER,
				       0);

   if (hStdout == INVALID_HANDLE_VALUE)
     return -1;

   if ((FALSE == SetConsoleActiveScreenBuffer(hStdout))
       || (FALSE == SetConsoleMode(hStdout, 0))
       || (-1 == win32_resize ()))
     {
	SLtt_reset_video ();
	return -1;
     }

   return 0;
}

int SLtt_init_video (void)
{
   SLtt_reset_video ();

   if (-1 == win32_init ())
     return -1;

   /* It is possible for SLtt_init_video to be called after suspension.
    * For all I know, the window size may have changed.  So, resize it
    * now.
    */

   Cursor_Row = Cursor_Col = 0;
   SLtt_Use_Ansi_Colors = IsColor = 1;
   Blink_Killed = 1;

   SLtt_reset_scroll_region ();
   goto_rc_abs (0, 0);
   fixup_colors ();

   return 0;
}

int SLtt_reset_video (void)
{
   if (hStdout == INVALID_HANDLE_VALUE)
     return 0;

   SLtt_reset_scroll_region ();
   SLtt_goto_rc (SLtt_Screen_Rows - 1, 0);
   Attribute_Byte = 0x7;
   Current_Color = JNO_COLOR;
   SLtt_del_eol ();
   (void) CloseHandle (hStdout);

   hStdout = GetStdHandle (STD_OUTPUT_HANDLE);
   if (hStdout != INVALID_HANDLE_VALUE)
     (void) SetConsoleActiveScreenBuffer(hStdout);

   hStdout = INVALID_HANDLE_VALUE;
   return 0;
}

#endif

/*}}}*/

#ifdef OS2_VIDEO /*{{{*/

# define INCL_BASE
# define INCL_NOPM
# define INCL_VIO
# define INCL_KBD

# define INCL_DOSPROCESS

# include <os2.h>
# ifndef __IBMC__
#  include <dos.h>
# endif
/* this is how to make a space character */
#define MK_SPACE_CHAR()	(((Attribute_Byte) << 8) | 0x20)

/* buffer to hold a line of character/attribute pairs */
#define MAXCOLS 256
static unsigned char Line_Buffer [MAXCOLS*2];

void SLtt_write_string (char *str)
{
   /* FIXME: Priority=medium
    * This should not go to stdout. */
   fputs (str, stdout);
}

void SLtt_goto_rc (int row, int col)
{
   row += Scroll_r1;
   VioSetCurPos (row, col, 0);
   Cursor_Row = row;
   Cursor_Col = col;
}

static void os2_video_getxy (void)
{
   USHORT r, c;

   VioGetCurPos (&r, &c, 0);
   Cursor_Row = r;
   Cursor_Col = c;
}

void SLtt_begin_insert (void)
{
   USHORT n;

   os2_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col;

   n = 2 * (n - 1);
   VioReadCellStr ((PCH)Line_Buffer, &n, Cursor_Row, Cursor_Col, 0);
   VioWrtCellStr ((PCH)Line_Buffer, n, Cursor_Row, Cursor_Col + 1, 0);
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
   USHORT n;

   os2_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col - 1;

   n *= 2;
   VioReadCellStr ((PCH)Line_Buffer, &n, Cursor_Row, Cursor_Col + 1, 0);
   VioWrtCellStr ((PCH)Line_Buffer, n, Cursor_Row, Cursor_Col, 0);
}

void SLtt_erase_line (void)
{
   USHORT w;

   Attribute_Byte = 0x07;
   w = MK_SPACE_CHAR ();

   VioWrtNCell ((BYTE*)&w, SLtt_Screen_Cols, Cursor_Row, 0, 0);

   Current_Color = JNO_COLOR;		/* since we messed with attribute byte */
}

void SLtt_delete_nlines (int nlines)
{
   SLtt_normal_video ();

   Line_Buffer[0] = ' '; Line_Buffer[1] = Attribute_Byte;
   VioScrollUp (Scroll_r1, 0, Scroll_r2, SLtt_Screen_Cols-1,
		nlines, (PCH) Line_Buffer, 0);
}

void SLtt_reverse_index (int nlines)
{
   SLtt_normal_video ();

   Line_Buffer[0] = ' '; Line_Buffer[1] = Attribute_Byte;
   VioScrollDn (Scroll_r1, 0, Scroll_r2, SLtt_Screen_Cols-1,
		nlines, (PCH) Line_Buffer, 0);
}

static void os2_video_invert_region (int top_row, int bot_row)
{
   int row, col;
   USHORT length = SLtt_Screen_Cols * 2;

   for (row = top_row; row < bot_row; row++)
     {
	VioReadCellStr ((PCH)Line_Buffer, &length, row, 0, 0);
	for (col = 1; col < length; col += 2)
	  Line_Buffer [col] ^= 0xff;
	VioWrtCellStr ((PCH)Line_Buffer, length, row, 0, 0);
     }
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);

   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) os2_video_invert_region (special, visual);
   if (audible) DosBeep (1500, 100); else DosSleep (100);
   if (visual) os2_video_invert_region (special, visual);
}

void SLtt_del_eol (void)
{
   USHORT w;
   if (Current_Color != JNO_COLOR) SLtt_normal_video ();

   w = MK_SPACE_CHAR ();

   VioWrtNCell ((BYTE*)&w, (SLtt_Screen_Cols - Cursor_Col),
		Cursor_Row, Cursor_Col, 0);
}

static void
write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   register unsigned char *p = Line_Buffer;
   register unsigned short pair;
   int n = count;

   /* write into a character/attribute pair */
   while (n-- > 0)
     {
	pair = SLSMG_CHAR_TO_USHORT(*src);/* character/color pair */
	src++;
	SLtt_reverse_video (pair >> 8);	/* color change */
	*(p++) = pair & 0xff;		/* character byte */
	*(p++) = Attribute_Byte;	/* attribute byte */
     }

   VioWrtCellStr ((PCH)Line_Buffer, (USHORT)(2 * count),
		  (USHORT)Cursor_Row, (USHORT)Cursor_Col, 0);
}

void SLtt_cls (void)
{
   SLtt_normal_video ();
   Line_Buffer [0] = ' '; Line_Buffer [1] = Attribute_Byte;
   VioScrollUp (0, 0, -1, -1, -1, (PCH)Line_Buffer, 0);
}

void SLtt_putchar (char ch)
{
   unsigned short p, *pp;

   if (Current_Color) SLtt_normal_video ();
   os2_video_getxy ();		/* get current position */

   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:			/* write character to screen */
 	VioWrtCharStrAtt (&ch, 1, Cursor_Row, Cursor_Col,
 			  (BYTE*)&Attribute_Byte, 0);
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_screen_size (void)
{
#ifdef __os2__
# ifdef __IBMC__
   VIOMODEINFO vioModeInfo;
# endif
   vioModeInfo.cb = sizeof(vioModeInfo);
   VioGetMode (&vioModeInfo, 0);
   SLtt_Screen_Cols = vioModeInfo.col;
   SLtt_Screen_Rows = vioModeInfo.row;
#endif
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

int SLtt_init_video (void)
{
   VIOINTENSITY RequestBlock;
   PTIB   ptib;
   PPIB   ppib;
   USHORT args[3] = { 6, 2, 1 };

   Cursor_Row = Cursor_Col = 0;

   IsColor = 1;			/* is it really? */

   /* Enable high-intensity background colors */
   RequestBlock.cb = sizeof (RequestBlock);
   RequestBlock.type = 2; RequestBlock.fs = 1;
   VioSetState (&RequestBlock, 0);	/* nop if !fullscreen */

   DosGetInfoBlocks (&ptib, &ppib);
   if ((ppib->pib_ultype) == 2)        /* VIO */
     Blink_Killed = 1;
   else
     {                              /* Fullscreen */
        if (VioSetState (args, 0) == 0)
	  Blink_Killed = 1;
        else
	  Blink_Killed = 0;
     }

   if (!Attribute_Byte)
     {
	/* find the attribute currently under the cursor */
	USHORT len, r, c;

	len = 2;
	VioGetCurPos (&r, &c, 0);
	VioReadCellStr ((PCH)Line_Buffer, &len, r, c, 0);
	Attribute_Byte = Line_Buffer[1];

	SLtt_set_color (JNORMAL_COLOR, NULL,
			Color_Names[(Attribute_Byte) & 0xf],
			Color_Names[(Attribute_Byte) >> 4]);
     }

   SLtt_Use_Ansi_Colors = IsColor;
   SLtt_get_screen_size ();
   SLtt_reset_scroll_region ();
   fixup_colors ();

   return 0;
}

#endif				       /* OS2_VIDEO */
/*}}}*/

#ifdef WATCOM_VIDEO /*{{{*/

# include <graph.h>
# define int86	int386		/* simplify code writing */

#include <dos.h>

/* this is how to make a space character */
#define MK_SPACE_CHAR()	(((Attribute_Byte) << 8) | 0x20)

/* buffer to hold a line of character/attribute pairs */
#define MAXCOLS 256
static unsigned char Line_Buffer [MAXCOLS*2];

/* define for direct to memory screen writes */
static unsigned char *Video_Base;
#define MK_SCREEN_POINTER(row,col) \
  ((unsigned short *)(Video_Base + 2 * (SLtt_Screen_Cols * (row) + (col))))

#define ScreenPrimary	(0xb800 << 4)
#define ScreenSize	(SLtt_Screen_Cols * SLtt_Screen_Rows)
#define ScreenSetCursor(x,y) _settextposition (x+1,y+1)

void ScreenGetCursor (int *x, int *y)
{
   struct rccoord rc = _gettextposition ();
   *x = rc.row - 1;
   *y = rc.col - 1;
}

void ScreenRetrieve (unsigned char *dest)
{
   memcpy (dest, (unsigned char *) ScreenPrimary, 2 * ScreenSize);
}

void ScreenUpdate (unsigned char *src)
{
   memcpy ((unsigned char *) ScreenPrimary, src, 2 * ScreenSize);
}

void SLtt_write_string (char *str)
{
   /* FIXME: Priority=medium
    * This should not go to stdout. */
   fputs (str, stdout);
}

void SLtt_goto_rc (int row, int col)
{
   row += Scroll_r1;
   if (row > SLtt_Screen_Rows) row = SLtt_Screen_Rows;
   if (col > SLtt_Screen_Cols) col = SLtt_Screen_Cols;
   ScreenSetCursor(row, col);
   Cursor_Row = row;
   Cursor_Col = col;
}

static void watcom_video_getxy (void)
{
   ScreenGetCursor (&Cursor_Row, &Cursor_Col);
}

void SLtt_begin_insert (void)
{
   unsigned short *p;
   unsigned short *pmin;
   int n;

   watcom_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col;

   p = MK_SCREEN_POINTER (Cursor_Row, SLtt_Screen_Cols - 1);
   pmin = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);

   while (p-- > pmin) *(p + 1) = *p;
}

void SLtt_end_insert (void)
{
}

void SLtt_delete_char (void)
{
   unsigned short *p;
   register unsigned short *p1;
   int n;

   watcom_video_getxy ();
   n = SLtt_Screen_Cols - Cursor_Col - 1;

   p = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);
   while (n--)
     {
	p1 = p + 1;
	*p = *p1;
	p++;
     }
}

void SLtt_erase_line (void)
{
   unsigned short w;
   unsigned short *p = MK_SCREEN_POINTER (Cursor_Row, 0);
   register unsigned short *pmax = p + SLtt_Screen_Cols;

   Attribute_Byte = 0x07;
   w = MK_SPACE_CHAR ();
   while (p < pmax) *p++ = w;
   Current_Color = JNO_COLOR;		/* since we messed with attribute byte */
}

void SLtt_delete_nlines (int nlines)
{
   union REGS r;

   SLtt_normal_video ();
   r.x.eax = nlines;
   r.x.ecx = 0;
   r.h.ah = 6;
   r.h.ch = Scroll_r1;
   r.h.dl = SLtt_Screen_Cols - 1;
   r.h.dh = Scroll_r2;
   r.h.bh = Attribute_Byte;
   int86 (0x10, &r, &r);
}

void SLtt_reverse_index (int nlines)
{
   union REGS r;

   SLtt_normal_video ();
   r.h.al = nlines;
   r.x.ecx = 0;
   r.h.ah = 7;
   r.h.ch = Scroll_r1;
   r.h.dl = SLtt_Screen_Cols - 1;
   r.h.dh = Scroll_r2;
   r.h.bh = Attribute_Byte;
   int86 (0x10, &r, &r);
}

static void watcom_video_invert_region (int top_row, int bot_row)
{
   unsigned char buf [2 * 180 * 80];         /* 180 cols x 80 rows */
   unsigned char *b, *bmax;

   b    = buf + 1 + 2 * SLtt_Screen_Cols * top_row;
   bmax = buf + 1 + 2 * SLtt_Screen_Cols * bot_row;
   ScreenRetrieve (buf);
   while (b < bmax)
     {
	*b ^= 0xFF;
	b += 2;
     }
   ScreenUpdate (buf);
}

void SLtt_beep (void)
{
   int audible;			/* audible bell */
   int special = 0;		/* first row to invert */
   int visual = 0;		/* final row to invert */

   if (!SLtt_Ignore_Beep) return;

   audible = (SLtt_Ignore_Beep & 1);
   if ( (SLtt_Ignore_Beep & 4) )
     {
	special = SLtt_Screen_Rows - 1;
	visual = special--;	/* only invert bottom status line */
     }
   else if ( (SLtt_Ignore_Beep & 2) )
     {
	visual = SLtt_Screen_Rows;
     }

   if (visual) watcom_video_invert_region (special, visual);
   if (audible) sound (1500); delay (100); if (audible) nosound ();
   if (visual) watcom_video_invert_region (special, visual);
}

void SLtt_del_eol (void)
{
   unsigned short *p, *pmax;
   unsigned short w;
   int n;

   n = SLtt_Screen_Cols - Cursor_Col;
   p = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);
   pmax = p + n;

   if (Current_Color != JNO_COLOR) SLtt_normal_video ();

   w = MK_SPACE_CHAR ();
   while (p < pmax) *p++ = w;
}

static void
write_attributes (SLsmg_Char_Type *src, unsigned int count)
{
   register unsigned short pair;
   register unsigned short *pos = MK_SCREEN_POINTER (Cursor_Row, 0);

   /* write into a character/attribute pair */
   while (count--)
     {
	pair = SLSMG_CHAR_TO_USHORT(*src);/* character/color pair */
	src++;
	SLtt_reverse_video (pair >> 8);	/* color change */
	*(pos++) = ((unsigned short) Attribute_Byte << 8) | (pair & 0xff);
     }
}

void SLtt_cls (void)
{
   SLtt_normal_video ();
   SLtt_reset_scroll_region ();
   SLtt_goto_rc (0, 0);
   SLtt_delete_nlines (SLtt_Screen_Rows);
}

void SLtt_putchar (char ch)
{
   unsigned short p, *pp;

   if (Current_Color) SLtt_normal_video ();
   watcom_video_getxy ();
   switch (ch)
     {
      case 7:			/* ^G - break */
	SLtt_beep (); break;
      case 8:			/* ^H - backspace */
	goto_rc_abs (Cursor_Row, Cursor_Col - 1); break;
      case 13:			/* ^M - carriage return */
	goto_rc_abs (Cursor_Row, 0); break;
      default:			/* write character to screen */
	p = (Attribute_Byte << 8) | (unsigned char) ch;
	pp = MK_SCREEN_POINTER (Cursor_Row, Cursor_Col);
	*pp = p;
	goto_rc_abs (Cursor_Row, Cursor_Col + 1);
     }
}

void SLtt_get_screen_size (void)
{
   struct videoconfig vc;
   _getvideoconfig(&vc);

   SLtt_Screen_Rows = vc.numtextrows;
   SLtt_Screen_Cols = vc.numtextcols;
}

void SLtt_get_terminfo (void)
{
   SLtt_get_screen_size ();
}

int SLtt_init_video (void)
{
#ifdef HAS_SAVE_SCREEN
   save_screen ();
#endif

   Cursor_Row = Cursor_Col = 0;
   Video_Base = (unsigned char *) ScreenPrimary;
   if (!Attribute_Byte) Attribute_Byte = 0x17;
   IsColor = 1;			/* is it really? */

   if (IsColor)
     {
	union REGS r;
	r.x.eax = 0x1003; r.x.ebx = 0;
	int86 (0x10, &r, &r);
	Blink_Killed = 1;
     }

   SLtt_Use_Ansi_Colors = IsColor;
   SLtt_get_screen_size ();
   SLtt_reset_scroll_region ();
   fixup_colors ();

   return 0;
}

#endif				       /* WATCOM_VIDEO */

/*}}}*/

/* -------------------------------------------------------------------------*\
 * The rest of the functions are, for the most part, independent of a specific
 * video system.
\* ------------------------------------------------------------------------ */

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_set_scroll_region (int r1, int r2);
 *
 * define a scroll region of top_row to bottom_row
\*----------------------------------------------------------------------*/
void SLtt_set_scroll_region (int top_row, int bottom_row)
{
   Scroll_r1 = top_row;
   Scroll_r2 = bottom_row;
}

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_reset_scroll_region (void);
 *
 * reset the scrol region to be the entire screen,
 * ie, SLtt_set_scroll_region (0, SLtt_Screen_Rows);
\*----------------------------------------------------------------------*/
void SLtt_reset_scroll_region (void)
{
   Scroll_r1 = 0;
   Scroll_r2 = SLtt_Screen_Rows - 1;
}

/*----------------------------------------------------------------------*\
 * Function:	int SLtt_flush_output (void);
\*----------------------------------------------------------------------*/
int SLtt_flush_output (void)
{
#if defined(WIN32_VIDEO)
   return 0;
#else
   /* FIXME: Priority=medium
    * This should not go to stdout. */
   fflush (stdout);
   return 0;
#endif
}

int SLtt_set_cursor_visibility (int show)
{
#if defined(WIN32_VIDEO)
   CONSOLE_CURSOR_INFO c;
     
   if (0 == GetConsoleCursorInfo (hStdout, &c))
     return -1;
   c.bVisible = (show ? TRUE: FALSE);
   if (0 == SetConsoleCursorInfo (hStdout, &c))
     return -1;
   return 0;
#else
   (void) show;
   return -1;
#endif
}

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_reverse_video (int color);
 *
 * set Attribute_Byte corresponding to COLOR.
 * Use Current_Color to remember the color which was set.
 * convert from the COLOR number to the attribute value.
\*----------------------------------------------------------------------*/
void SLtt_reverse_video (int color)
{
   if ((color >= JMAX_COLORS) || (color < 0))
     return;

   Attribute_Byte = Color_Map [color];
   Current_Color = color;
}

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_normal_video (void);
 *
 * reset the attributes for normal video
\*----------------------------------------------------------------------*/
void SLtt_normal_video (void)
{
   SLtt_reverse_video (JNORMAL_COLOR);
}

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_smart_puts (SLsmg_Char_Type *new_string,
 *				      SLsmg_Char_Type *old_string,
 *				      int len, int row);
 *
 * puts NEW_STRING, which has length LEN, at row ROW.  NEW_STRING contains
 * characters/colors packed in the form value = ((color << 8) | (ch));
 *
 * the puts tries to avoid overwriting the same characters/colors
 *
 * OLD_STRING is not used, maintained for compatibility with other systems
\*----------------------------------------------------------------------*/
void SLtt_smart_puts (SLsmg_Char_Type *new_string,
		      SLsmg_Char_Type *old_string,
		      int len, int row)
{
   (void) old_string;
   Cursor_Row = row;
   Cursor_Col = 0;
   write_attributes (new_string, len);
}

/*----------------------------------------------------------------------*\
 * Function:	int SLtt_reset_video (void);
\*----------------------------------------------------------------------*/
#ifndef WIN32_VIDEO
int SLtt_reset_video (void)
{
   SLtt_reset_scroll_region ();
   SLtt_goto_rc (SLtt_Screen_Rows - 1, 0);
#ifdef HAS_SAVE_SCREEN
   restore_screen ();
#endif
   Attribute_Byte = 0x07;
   Current_Color = JNO_COLOR;
   SLtt_del_eol ();
   return 0;
}
#endif

/*----------------------------------------------------------------------*\
 * Function:	void SLtt_set_color (int obj, char *what, char *fg, char *bg);
 *
 * set foreground and background colors of OBJ to the attributes which
 * correspond to the names FG and BG, respectively.
 *
 * WHAT is the name corresponding to the object OBJ, but is not used in
 * this routine.
\*----------------------------------------------------------------------*/
void SLtt_set_color (int obj, char *what, char *fg, char *bg)
{
   int i, b = 0, f = 7;

   (void) what;

   if ((obj < 0) || (obj >= JMAX_COLORS))
     return;

   for (i = 0; i < JMAX_COLOR_NAMES; i++ )
     {
	if (!strcmp (fg, Color_Names [i]))
	  {
	     f = i;
	     break;
	  }
     }

   for (i = 0; i < JMAX_COLOR_NAMES; i++)
     {
	if (!strcmp (bg, Color_Names [i]))
	  {
	     if (Blink_Killed) b = i; else b = i & 0x7;
	     break;
	  }
     }
   if (f == b) return;

   Color_Map [obj] = (b << 4) | f;

    /* if we're setting the normal color, and the attribute byte hasn't
     been set yet, set it to the new color */
   if ((obj == 0) && (Attribute_Byte == 0))
     SLtt_reverse_video (0);
   
   if (_SLtt_color_changed_hook != NULL)
     (*_SLtt_color_changed_hook)();
}

static void fixup_colors (void)
{
   unsigned int i;

   if (Blink_Killed)
     return;

   for (i = 0; i < JMAX_COLORS; i++)
     Color_Map[i] &= 0x7F;

   SLtt_normal_video ();
}


/* FIXME!!! Add mono support.
 * The following functions have not been fully implemented.
 */
void SLtt_set_mono (int obj_unused, char *unused, SLtt_Char_Type c_unused)
{
   (void) obj_unused;
   (void) unused;
   (void) c_unused;
}

#if 0
void SLtt_add_color_attribute (int obj, SLtt_Char_Type attr)
{
   (void) obj;
   (void) attr;
}
#endif
