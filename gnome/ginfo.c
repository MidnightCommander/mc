/*
 *
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "dlg.h"
#include "widget.h"
#include "info.h"
#include "win.h"
#include "x.h"

/* The following three include files are needed for cpanel */
#include "main.h"
#include "dir.h"
#include "panel.h"

void
x_create_info (Dlg_head *h, widget_data parent, WInfo *info)
{
	printf ("create info\n");
}

void
x_show_info (WInfo *info, struct my_statfs *s, struct stat *b)
{
	printf ("updating info\n");
}
