/* Properties dialog for the Gnome edition of the Midnight Commander
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 */

#ifndef GPROP_H
#define GPROP_H


#include <sys/stat.h>
#include <gtk/gtk.h>
#include "config.h"


/***** Filename *****/

typedef struct {
	GtkWidget *top;

	GtkWidget *filename;
} GpropFilename;

GpropFilename *gprop_filename_new (char *complete_filename, char *filename);
void gprop_filename_get_data (GpropFilename *gp, char **filename);

/***** Permissions *****/

typedef struct {
	GtkWidget *top;

	GtkWidget *mode_label;

	GtkWidget *suid, *sgid, *svtx;
	GtkWidget *rusr, *wusr, *xusr;
	GtkWidget *rgrp, *wgrp, *xgrp;
	GtkWidget *roth, *woth, *xoth;

	GtkWidget *owner;
	GtkWidget *group;
} GpropPerm;

GpropPerm *gprop_perm_new (umode_t umode, char *owner, char *group);
void gprop_perm_get_data (GpropPerm *gp, umode_t *umode, char **owner, char **group);

/***** General *****/

typedef struct {
	GtkWidget *top;

	GtkWidget *title;
	GtkWidget *icon_filename;
	GtkWidget *icon_pixmap;
} GpropGeneral;

GpropGeneral *gprop_general_new (char *title, char *icon_filename);
void gprop_general_get_data (GpropGeneral *gp, char **title, char **icon_filename);

typedef struct {
	GtkWidget *top;
	GtkWidget *entry;
	GtkWidget *check;
} GpropExec;

GpropExec *gprop_exec_new     (GnomeDesktopEntry *dentry);
void      gprop_exec_get_data (GpropExec *ge, GnomeDesktopEntry *dentry);

#endif
