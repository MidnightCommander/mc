/* Ch-Drive command for Windows NT operating system
   
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
   
   Bug:
   	the code will not work if you have more drives than those that
   	can fit in a panel.  
   */

#ifndef _OS_NT
#error This file is for the Win32 operating systems.
#else

#include <config.h>
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "tty.h"
#include "mad.h"
#include "util.h"
#include "win.h"
#include "color.h"
#include "dlg.h"
#include "widget.h"
#include "dialog.h"	/* For do_refresh() */

#include "dir.h"
#include "panel.h"		/* Needed for the externs */
#include "file.h"
#include "main.h"
#include "util.Win32.h"

struct Dlg_head *drive_dlg;
WPanel *this_panel;

static int drive_dlg_callback (Dlg_head *h, int Par, int Msg);
static void drive_dlg_refresh (void);
static void drive_cmd();

#define B_DRIVE_BASE 100

void drive_cmd_a()
{
    this_panel = left_panel;
    drive_cmd();
}

void drive_cmd_b()
{                                  
    this_panel = right_panel;
    drive_cmd();
}

void drive_chg(WPanel *panel)
{
    this_panel  = panel;
    drive_cmd();
}

#define MAX_LGH 13		/* Length for drives */

static void drive_cmd()			
{
    int  i, nNewDrive, nDrivesAvail;
    char szTempBuf[7], szDrivesAvail[256], *p;	/* mmm... this is bad practice... (256) */

	// Dialogbox position 
	int  x_pos;									/* X-Position for the dialog */
	int  y_pos = (LINES-6)/2-3;                 /* Center on y */
	int  y_height;
	int  x_width;

	int  m_drv;

    /* Get drives name and count */
    GetLogicalDriveStrings (255, szDrivesAvail);
    for (nDrivesAvail=0, p=szDrivesAvail; *p; nDrivesAvail++) 
    	p+=4;

    /* Create Dialog */
    do_refresh ();
	
	m_drv = ((nDrivesAvail > MAX_LGH) ? MAX_LGH: nDrivesAvail);
	x_pos = this_panel->widget.x + (this_panel->widget.cols - m_drv*3)/2 + 2;	/* Center on x, relative to panel */

	if (nDrivesAvail > MAX_LGH) {
		y_height = 8;
		x_width  = 33;
	} else {
	    y_height = 6;
		x_width  = (nDrivesAvail - 1) * 2 + 9;
	}

    drive_dlg = create_dlg (y_pos, x_pos,
				   			y_height, 
							x_width, 
							dialog_colors,
							drive_dlg_callback, 
							"[ChDrive]", 
							"drive", 
							DLG_NONE);

    x_set_dialog_title (drive_dlg, "Change Drive");

	if (nDrivesAvail>MAX_LGH) {
		for (i = 0; i < nDrivesAvail - MAX_LGH; i++) {
			p -= 4;
			sprintf(szTempBuf, "&%c", *p);
		    add_widgetl(drive_dlg, 
			  button_new (5, 
						  (m_drv-i-1)*2+4 - (MAX_LGH*2 - nDrivesAvail) * 2, 
						  B_DRIVE_BASE + nDrivesAvail - i - 1, 
						  HIDDEN_BUTTON,
						  szTempBuf, 0, NULL, NULL), 
						  XV_WLAY_RIGHTOF);
		}
	}

    /* Add a button for each drive */
    for (i = 0; i < m_drv; i++) {
    	p -= 4;
    	sprintf (szTempBuf, "&%c", *p);
	    add_widgetl(drive_dlg, 
					button_new (3, 
								(m_drv-i-1)*2+4, 
								B_DRIVE_BASE+m_drv-i-1, 
								HIDDEN_BUTTON,
								szTempBuf, 
								0, 
								NULL, 
								NULL), 
					XV_WLAY_RIGHTOF);
    }

    run_dlg(drive_dlg);   

    /* do action */
    if (drive_dlg->ret_value != B_CANCEL) {
		int  errocc = 0; // no error
		int  rtn;
		char drvLetter;

		nNewDrive = drive_dlg->ret_value - B_DRIVE_BASE;
		drvLetter =  (char) *(szDrivesAvail + (nNewDrive*4));
		if (win32_GetPlatform() == OS_WinNT) {	/* Windows NT */
			rtn = _chdrive(drvLetter - 'A' + 1);
		} else {				/* Windows 95 */
			// HANDLE hDevice;
			rtn = 1;
			SetCurrentDirectory(szDrivesAvail+(nNewDrive*4));
			/* Bug: can only change to DRV:\ */
			// hDevice = CreateFile("\\\\.\\VWIN32", 0, 0, NULL, 0, 0, NULL);
			// CloseHandle(hDevice);
		}
		if (rtn == -1) 
			errocc = 1;
		else {
			getcwd (this_panel->cwd, sizeof (this_panel->cwd)-2);
            if (toupper(drvLetter) == toupper(*(this_panel->cwd)))  {
  				clean_dir (&this_panel->dir, this_panel->count);
				this_panel->count = do_load_dir(&this_panel->dir, 
												this_panel->sort_type, 
												this_panel->reverse, 
												this_panel->case_sensitive, 
												this_panel->filter);
				this_panel->top_file = 0;
				this_panel->selected = 0;
				this_panel->marked = 0;
				this_panel->total = 0;
				show_dir(this_panel);
				reread_cmd();
			} else  errocc = 1;
		}
	    if (errocc)
			message (1, " Error ", 
					 "Can't access drive %c: \n", 
					 *(szDrivesAvail+(nNewDrive*4)) );
    }
    destroy_dlg (drive_dlg);
    repaint_screen ();


}

static int drive_dlg_callback (Dlg_head *h, int Par, int Msg)
{
    char buffer [10];
    
    switch (Msg) {
#ifndef HAVE_X
    case DLG_DRAW:
	drive_dlg_refresh ();
	break;
#endif
    }
    return 0;
}

static void drive_dlg_refresh (void)
{
    attrset (dialog_colors[0]);
    dlg_erase (drive_dlg);
    draw_box (drive_dlg, 1, 1, drive_dlg->lines-2, drive_dlg->cols-2);

    attrset (dialog_colors[2]);
    dlg_move (drive_dlg, 1, drive_dlg->cols/2 - 7);
    addstr (" Change Drive ");
}

#endif
