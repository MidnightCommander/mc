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


/***** General *****/

typedef struct {
	GtkWidget *top;

	GtkWidget *filename;
} GpropGeneral;

GpropGeneral *gprop_general_new (char *complete_filename, char *filename);
void gprop_general_get_data (GpropGeneral *gpg, char **filename);

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
void gprop_perm_get_data (GpropPerm *gpp, umode_t *umode, char **owner, char **group);

/***** Directory *****/

typedef struct {
	GtkWidget *top;

	GtkWidget *icon_filename;
} GpropDir;

GpropDir *gprop_dir_new (char *icon_filename);
void gprop_dir_get_data (GpropDir *gpd, char **icon_filename);

#endif
