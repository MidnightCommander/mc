/* Client interface for General purpose Win32 console save/restore server
   Having the same interface as its Linux counterpart:
   	Copyright (C) 1994 Janne Kukonlehto <jtklehto@stekt.oulu.fi> 
   
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
   
   Note:
   	show_console_contents doesn't know how to write to its window
   	the rest works fine.
*/
#include <config.h>
#ifndef _OS_NT
#error This file is for Win32 operating systems.
#else

#include <windows.h>
#include "util.debug.h"

int    cons_saver_pid = 1;

#include "tty.h"
#include "util.h"
#include "win.h"
#include "cons.saver.h"

signed char console_flag = 1;
static HANDLE hSaved, hNew;

void show_console_contents (int starty, unsigned char begin_line, unsigned char end_line)
{
    DWORD dw;
    COORD c0 = { 0, 0 };
    COORD csize; 
    SMALL_RECT rect;
    CHAR_INFO *pchar;

    csize.X = COLS;
    csize.Y = end_line-begin_line;
    rect.Left = 0;
    rect.Top = begin_line;
    rect.Right = COLS;
    rect.Bottom = end_line;

//    -- This code reads characters and attributes
    pchar = malloc (sizeof(CHAR_INFO) *  (end_line-begin_line) * COLS);
    // Copy from one console to the curses virtual screen
    win32APICALL(ReadConsoleOutput (hSaved, pchar, csize, c0, &rect));

    // FIXME: this should've work, but refresh() is called after this write :-(
    win32APICALL(WriteConsoleOutput (hNew, pchar, csize, c0, &rect));
    
#ifdef USE_NCURSES		
// Here we read only characters so that we can printw to stdscr.
//    pchar = malloc (sizeof(TCHAR) * (end_line-begin_line) * COLS);

    // Copy from one console to the curses virtual screen
//    ReadConsoleOutputCharacter (hSaved, pchar, (end_line-begin_line) * COLS, c0, &dw);

//    mvprintw(0, begin_line, "%.*s", (end_line-begin_line) * COLS, pchar);
#else
//#error show_console_contents not written for S-Lang
#endif


    free (pchar);
}

void handle_console (unsigned char action)
{
    static SECURITY_ATTRIBUTES sa;
    COORD c;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    long    lMode;

    switch (action){
    case CONSOLE_INIT:			// Init
    	hSaved = GetStdHandle (STD_OUTPUT_HANDLE);		// Save Standard handle

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;				// Create a new console buffer
	win32APICALL_HANDLE(hNew, /* = */ CreateConsoleScreenBuffer (GENERIC_WRITE | GENERIC_READ, 
				FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, 
				CONSOLE_TEXTMODE_BUFFER, NULL));
	win32APICALL(GetConsoleScreenBufferInfo(hSaved, &csbi));	// ... with same size 
	win32APICALL(SetConsoleScreenBufferSize(hNew, csbi.dwSize));

	win32APICALL(SetConsoleActiveScreenBuffer(hNew));		// ... that becomes standard handle
	win32APICALL(SetConsoleMode(hNew, ENABLE_PROCESSED_INPUT));
	win32APICALL(SetStdHandle(STD_OUTPUT_HANDLE, hNew));
	break;

    case CONSOLE_DONE:			// Clean Up
    	win32APICALL(CloseHandle (hNew));
    	break;

    case CONSOLE_SAVE:			// Save
	win32APICALL(SetConsoleActiveScreenBuffer (hNew));		// Current = our standard handle
	win32APICALL(SetStdHandle (STD_OUTPUT_HANDLE, hNew));
	break;

    case CONSOLE_RESTORE:		// Restore
	win32APICALL(SetConsoleActiveScreenBuffer (hSaved));		// Put saved (shell) screen buffer 
	win32APICALL(SetStdHandle (STD_OUTPUT_HANDLE, hSaved));
	break;
    default:
	win32Trace(("Invalid action code %d received in handle_console", action));
    }
}

#endif // !_OS_NT
