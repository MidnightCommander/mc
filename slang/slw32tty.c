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

#include "slinclud.h"

#include <windows.h>
#include <winbase.h>

#include "slang.h"
#include "_slang.h"

#ifdef __cplusplus
# define _DOTS_ ...
#else
# define _DOTS_ void
#endif

static int Process_Mouse_Events;

/*----------------------------------------------------------------------*\
 *  Function:	static void set_ctrl_break (int state);
 *
 * set the control-break setting
\*----------------------------------------------------------------------*/
static void set_ctrl_break (int state)
{
}

/*----------------------------------------------------------------------*\
 *  Function:	int SLang_init_tty (int abort_char, int no_flow_control,
 *				    int opost);
 *
 * initialize the keyboard interface and attempt to set-up the interrupt 9
 * handler if ABORT_CHAR is non-zero.
 * NO_FLOW_CONTROL and OPOST are only for compatiblity and are ignored.
\*----------------------------------------------------------------------*/

HANDLE SLw32_Hstdin = INVALID_HANDLE_VALUE;

int SLang_init_tty (int abort_char, int no_flow_control, int opost)
{
   (void) opost;
   (void) no_flow_control;

   if (SLw32_Hstdin != INVALID_HANDLE_VALUE)
     return 0;

#if 1
   /* stdin may have been redirected.  So try this */
   SLw32_Hstdin = CreateFile ("CONIN$", GENERIC_READ|GENERIC_WRITE, 
			       FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, 
			       OPEN_EXISTING, 0, NULL);
   if (SLw32_Hstdin == INVALID_HANDLE_VALUE)
     return -1;
#else
   if (INVALID_HANDLE_VALUE == (SLw32_Hstdin = GetStdHandle(STD_INPUT_HANDLE)))
     return -1;
#endif

   if (FALSE == SetConsoleMode(SLw32_Hstdin, ENABLE_WINDOW_INPUT|ENABLE_MOUSE_INPUT))
     {
	SLw32_Hstdin = INVALID_HANDLE_VALUE;
	return -1;
     }
   
   if (abort_char > 0)
     SLang_Abort_Char = abort_char;

   return 0;
}
/* SLang_init_tty */

/*----------------------------------------------------------------------*\
 *  Function:	void SLang_reset_tty (void);
 *
 * reset the tty before exiting
\*----------------------------------------------------------------------*/
void SLang_reset_tty (void)
{
   SLw32_Hstdin = INVALID_HANDLE_VALUE;
   set_ctrl_break (1);
}

static int process_mouse_event (MOUSE_EVENT_RECORD *m)
{
   char buf [8];

   if (Process_Mouse_Events == 0)
     return -1;

   if (m->dwEventFlags)
     return -1;			       /* double click or movement event */

   /* A button was either pressed or released.  Now make sure that
    * the shift keys were not also pressed.
    */
   if (m->dwControlKeyState
       & (LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED
	  |LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED
	  |SHIFT_PRESSED))
     return -1;

   /* We have a simple press or release.  Encode it as an escape sequence
    * and buffer the result.  The encoding is:
    *   'ESC [ M b x y'
    *  where b represents the button state, and x,y represent the coordinates.
    * The ESC is handled by the calling routine.
    */
   if (m->dwButtonState & 1) buf[3] = ' ';
   else if (m->dwButtonState & 2) buf[3] = ' ' + 2;
   else if (m->dwButtonState & 4) buf[3] = ' ' + 1;
   else return -1;

   buf[0] = 27;
   buf[1] = '[';
   buf[2] = 'M';


   buf[4] = 1 + ' ' + m->dwMousePosition.X;
   buf[5] = 1 + ' ' + m->dwMousePosition.Y;

   return SLang_buffer_keystring ((unsigned char *)buf, 6);
}

static int process_key_event (KEY_EVENT_RECORD *key)
{
   unsigned int key_state = 0;
   unsigned int scan;
   char c1;
   DWORD d = key->dwControlKeyState;
   unsigned char buf[4];

   if (!key->bKeyDown) return 0;
   if (d & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
     key_state |= _pSLTT_KEY_ALT;
   if (d & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) 
     key_state |= _pSLTT_KEY_CTRL;
   if (d & SHIFT_PRESSED)
     key_state |= _pSLTT_KEY_SHIFT;

   scan = key->wVirtualScanCode;

   switch (scan)
     {
      case 0x00E:		       /* backspace */
	return SLang_buffer_keystring ((unsigned char *)"\x7F", 1);

      case 0x003:		       /* 2 key */
	if (key_state & _pSLTT_KEY_ALT)
	  break;
	/* Drop */
      case 0x039: 		       /* space */
	if (key_state & _pSLTT_KEY_CTRL)
	  return SLang_buffer_keystring ((unsigned char *)"\x00\x03", 2);
	break;

      case 0x007:		       /* 6 key */
	if (_pSLTT_KEY_CTRL == (key_state & (_pSLTT_KEY_ALT|_pSLTT_KEY_CTRL)))
	  return SLang_buffer_keystring ((unsigned char *)"\x1E", 1);   /* Ctrl-^ */
	break;

      case 0x00C:		       /* -/_ key */
	if (_pSLTT_KEY_CTRL == (key_state & (_pSLTT_KEY_ALT|_pSLTT_KEY_CTRL)))
	  return SLang_buffer_keystring ((unsigned char *)"\x1F", 1);
	break;

      case 0x00F:		       /* TAB */
	if (_pSLTT_KEY_SHIFT == key_state)
	  return SLang_buffer_keystring ((unsigned char *)"\x00\x09", 2);
	break;

      case 0xE02F:		       /* KEYPAD SLASH */
      case 0x037:		       /* KEYPAD STAR */
      case 0x04A:		       /* KEYPAD MINUS */
      case 0x04E:		       /* KEYPAD PLUS */
	if (d & NUMLOCK_ON)
	  break;
      case 0x047:		       /* KEYPAD HOME */
      case 0x048:		       /* KEYPAD UP */
      case 0x049:		       /* KEYPAD PGUP */
      case 0x04B:		       /* KEYPAD LEFT */
      case 0x04C:		       /* KEYPAD 5 */
      case 0x04D:		       /* KEYPAD RIGHT */
      case 0x04F:		       /* KEYPAD END */
      case 0x050:		       /* KEYPAD DOWN */
      case 0x051:		       /* KEYPAD PGDN */
      case 0x052:		       /* KEYPAD INSERT */
      case 0x053:		       /* KEYPAD DEL */
	if (d & ENHANCED_KEY)
	  scan |= 0xE000;
	else
	  {
	     if (d & NUMLOCK_ON)
	       break;
	  }
	(void) _pSLpc_convert_scancode (scan, key_state, 0);
	return 0;

      case 0x3b:		       /* F1 */
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
      case 0x40:
      case 0x41:
      case 0x42:
      case 0x43:
      case 0x44:
      case 0x57:
      case 0x58:		       /* F12 */
	(void) _pSLpc_convert_scancode (scan, key_state, 0);
     }
   
   c1 = key->uChar.AsciiChar;
   if (c1 != 0)
     {
	if (_pSLTT_KEY_ALT == (key_state & (_pSLTT_KEY_ALT|_pSLTT_KEY_CTRL)))
	  {
	     buf[0] = 27;
	     buf[1] = c1;
	     return SLang_buffer_keystring (buf, 2);
	  }
	if (c1 == SLang_Abort_Char)
	  {
	     if (SLang_Ignore_User_Abort == 0) 
	       SLang_set_error (USER_BREAK);
	     SLKeyBoard_Quit = 1;
	  }
	buf[0] = c1;
	return SLang_buffer_keystring (buf, 1);
     }
   return 0;
}


static void process_console_records(void)
{
   INPUT_RECORD record;
   DWORD bytesRead;
   DWORD n = 0;

   if (FALSE == GetNumberOfConsoleInputEvents(SLw32_Hstdin, &n))
     return;

   while (n > 0)
     {
	ReadConsoleInput(SLw32_Hstdin, &record, 1, &bytesRead);
	switch (record.EventType)
	  {
	   case KEY_EVENT:
	     (void) process_key_event(&record.Event.KeyEvent);
	     break;

	   case MOUSE_EVENT:
	     process_mouse_event(&record.Event.MouseEvent);
	     break;

	   case WINDOW_BUFFER_SIZE_EVENT:
	     /* process_resize_records(&record.Event.WindowBufferSizeEvent); */
	     break;
	  }
	n--;
     }
}

/*----------------------------------------------------------------------*\
 *  Function:	int _pSLsys_input_pending (int tsecs);
 *
 *  sleep for *tsecs tenths of a sec waiting for input
\*----------------------------------------------------------------------*/
int _pSLsys_input_pending (int tsecs)
{
   long ms;

   if (SLw32_Hstdin == INVALID_HANDLE_VALUE)
     return -1;

   if (tsecs < 0) ms = -tsecs;	       /* specifies 1/1000 */
   else ms = tsecs * 100L;	       /* convert 1/10 to 1/1000 secs */

   process_console_records ();
   while ((ms > 0)
	  && (SLang_Input_Buffer_Len == 0))
     {
	long t;

	t = GetTickCount ();

	(void) WaitForSingleObject (SLw32_Hstdin, ms);
	process_console_records ();
	ms -= GetTickCount () - t;
     }
   
   return SLang_Input_Buffer_Len;
}

/*----------------------------------------------------------------------*\
 *  Function:	unsigned int _pSLsys_getkey (void);
 *
 * wait for and get the next available keystroke.
 * Also re-maps some useful keystrokes.
 *
 *	Backspace (^H)	=>	Del (127)
 *	Ctrl-Space	=>	^@	(^@^3 - a pc NUL char)
 *	extended keys are prefixed by a null character
\*----------------------------------------------------------------------*/
unsigned int _pSLsys_getkey (void)
{
   /* Check the input buffer because _pSLsys_input_pending may have been 
    * called prior to this to stuff the input buffer.
    */
   if (SLang_Input_Buffer_Len)
     return SLang_getkey ();

   if (SLw32_Hstdin == INVALID_HANDLE_VALUE)
     return SLANG_GETKEY_ERROR;

   while (1)
     {
	int status;

	if (SLKeyBoard_Quit)
	  return SLang_Abort_Char;
	
	status = _pSLsys_input_pending (600);
	if (status == -1)
	  return SLANG_GETKEY_ERROR;
	
	if (status > 0)
	  return SLang_getkey ();
     }
}

/*----------------------------------------------------------------------*\
 *  Function:	int SLang_set_abort_signal (void (*handler)(int));
\*----------------------------------------------------------------------*/
int SLang_set_abort_signal (void (*handler)(int))
{
   if (SLw32_Hstdin == INVALID_HANDLE_VALUE)
     return -1;
   if (handler == NULL)
     SLang_Interrupt = process_console_records;
   return 0;
}

int SLtt_set_mouse_mode (int mode, int force)
{
   (void) force;

   Process_Mouse_Events = mode;
   return 0;
}
