/* Copyright (c) 1992, 1999, 2001, 2002, 2003 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

unsigned int SLang_Input_Buffer_Len = 0;
unsigned char SLang_Input_Buffer [SL_MAX_INPUT_BUFFER_LEN];

int SLang_Abort_Char = 7;
int SLang_Ignore_User_Abort = 0;

/* This has the effect of mapping all characters in the range 128-169 to
 * ESC [ something
 */

unsigned int SLang_getkey (void)
{
   unsigned int imax;
   unsigned int ch;

   if (SLang_Input_Buffer_Len)
     {
	ch = (unsigned int) *SLang_Input_Buffer;
	SLang_Input_Buffer_Len--;
	imax = SLang_Input_Buffer_Len;

	SLMEMCPY ((char *) SLang_Input_Buffer,
		(char *) (SLang_Input_Buffer + 1), imax);
     }
   else if (SLANG_GETKEY_ERROR == (ch = _SLsys_getkey ())) return ch;

#if _SLANG_MAP_VTXXX_8BIT
# if !defined(IBMPC_SYSTEM)
   if (ch & 0x80)
     {
	unsigned char i;
	i = (unsigned char) (ch & 0x7F);
	if (i < ' ')
	  {
	     i += 64;
	     SLang_ungetkey (i);
	     ch = 27;
	  }
     }
# endif
#endif
   return(ch);
}

int SLang_ungetkey_string (unsigned char *s, unsigned int n)
{
   register unsigned char *bmax, *b, *b1;
   if (SLang_Input_Buffer_Len + n + 3 > SL_MAX_INPUT_BUFFER_LEN)
     return -1;

   b = SLang_Input_Buffer;
   bmax = (b - 1) + SLang_Input_Buffer_Len;
   b1 = bmax + n;
   while (bmax >= b) *b1-- = *bmax--;
   bmax = b + n;
   while (b < bmax) *b++ = *s++;
   SLang_Input_Buffer_Len += n;
   return 0;
}

int SLang_buffer_keystring (unsigned char *s, unsigned int n)
{

   if (n + SLang_Input_Buffer_Len + 3 > SL_MAX_INPUT_BUFFER_LEN) return -1;

   SLMEMCPY ((char *) SLang_Input_Buffer + SLang_Input_Buffer_Len,
	   (char *) s, n);
   SLang_Input_Buffer_Len += n;
   return 0;
}

int SLang_ungetkey (unsigned char ch)
{
   return SLang_ungetkey_string(&ch, 1);
}

int SLang_input_pending (int tsecs)
{
   int n;
   unsigned char c;
   if (SLang_Input_Buffer_Len) return (int) SLang_Input_Buffer_Len;

   n = _SLsys_input_pending (tsecs);

   if (n <= 0) return 0;

   c = (unsigned char) SLang_getkey ();
   SLang_ungetkey_string (&c, 1);

   return n;
}

void SLang_flush_input (void)
{
   int quit = SLKeyBoard_Quit;

   SLang_Input_Buffer_Len = 0;
   SLKeyBoard_Quit = 0;
   while (_SLsys_input_pending (0) > 0)
     {
	(void) _SLsys_getkey ();
	/* Set this to 0 because _SLsys_getkey may stuff keyboard buffer if
	 * key sends key sequence (OS/2, DOS, maybe VMS).
	 */
	SLang_Input_Buffer_Len = 0;
     }
   SLKeyBoard_Quit = quit;
}

#ifdef IBMPC_SYSTEM
static int Map_To_ANSI;
int SLgetkey_map_to_ansi (int enable)
{
   Map_To_ANSI = enable;
   return 0;
}

static int convert_scancode (unsigned int scan, 
			     unsigned int shift,
			     int getkey,
			     unsigned int *ret_key)
{
   unsigned char buf[16];
   unsigned char *b;
   unsigned char end;
   int is_arrow;

   shift &= (_SLTT_KEY_ALT|_SLTT_KEY_SHIFT|_SLTT_KEY_CTRL);

   b = buf;
   if (_SLTT_KEY_ALT == shift)
     {
	shift = 0;
	*b++ = 27;
     }
   *b++ = 27;
   *b++ = '[';

   is_arrow = 0;
   end = '~';
   if (shift)
     {
	if (shift == _SLTT_KEY_CTRL)
	  end = '^';
	else if (shift == _SLTT_KEY_SHIFT)
	  end = '$';
	else shift = 0;
     }

   /* These mappings correspond to what rxvt produces under Linux */
   switch (scan & 0xFF)
     {
      default:
	return -1;

      case 0x47:		       /* home */
	*b++ = '1';
	break;
      case 0x48:		       /* up */
	end = 'A';
	is_arrow = 1;
	break;
      case 0x49:		       /* PgUp */
	*b++ = '5';
	break;
      case 0x4B:		       /* Left */
	end = 'D';
	is_arrow = 1;
	break;
      case 0x4D:		       /* Right */
	end = 'C';
	is_arrow = 1;
	break;
      case 0x4F:		       /* End */
	*b++ = '4';
	break;
      case 0x50:		       /* Down */
	end = 'B';
	is_arrow = 1;
	break;
      case 0x51:		       /* PgDn */
	*b++ = '6';
	break;
      case 0x52:		       /* Insert */
	*b++ = '2';
	break;
      case 0x53:		       /* Delete */
	*b++ = '3';
	break;
      case ';':			       /* F1 */
	*b++ = '1';
	*b++ = '1';
	break;
      case '<':			       /* F2 */
	*b++ = '1';
	*b++ = '2';
	break;
      case '=':			       /* F3 */
	*b++ = '1';
	*b++ = '3';
	break;

      case '>':			       /* F4 */
	*b++ = '1';
	*b++ = '4';
	break;

      case '?':			       /* F5 */
	*b++ = '1';
	*b++ = '5';
	break;

      case '@':			       /* F6 */
	*b++ = '1';
	*b++ = '7';
	break;

      case 'A':			       /* F7 */
	*b++ = '1';
	*b++ = '8';
	break;

      case 'B':			       /* F8 */
	*b++ = '1';
	*b++ = '9';
	break;

      case 'C':			       /* F9 */
	*b++ = '2';
	*b++ = '0';
	break;

      case 'D':			       /* F10 */
	*b++ = '2';
	*b++ = '1';
	break;

      case 0x57:		       /* F11 */
	*b++ = '2';
	*b++ = '3';
	break;

      case 0x58:		       /* F12 */
	*b++ = '2';
	*b++ = '4';
	break;
     }

   if (is_arrow && shift)
     {
	if (shift == _SLTT_KEY_CTRL)
	  end &= 0x1F;
	else
	  end |= 0x20;
     }
   *b++ = end;
   
   if (getkey)
     {
	(void) SLang_buffer_keystring (buf + 1, (unsigned int) (b - (buf + 1)));
	*ret_key = buf[0];
	return 0;
     }

   (void) SLang_buffer_keystring (buf, (unsigned int) (b - buf));
   return 0;
}

   
unsigned int _SLpc_convert_scancode (unsigned int scan,
				     unsigned int shift,
				     int getkey)
{
   unsigned char buf[16];

   if (Map_To_ANSI)
     {
	if (0 == convert_scancode (scan, shift, getkey, &scan))
	  return scan;
     }
   
   if (getkey)
     {
	buf[0] = scan & 0xFF;
	SLang_buffer_keystring (buf, 1);
	return (scan >> 8) & 0xFF;
     }
   buf[0] = (scan >> 8) & 0xFF;
   buf[1] = scan & 0xFF;
   (void) SLang_buffer_keystring (buf, 2);
   return 0;
}

#endif
