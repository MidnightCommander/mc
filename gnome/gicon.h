#ifndef GNOME_GICON_H
#define GNOME_GICON_H

GdkImlibImage *gicon_get_by_filename         (char *fname);
GdkImlibImage *gicon_stock_load              (char *basename);
GdkImlibImage *gicon_get_icon_for_file       (file_entry *fe);
GdkImlibImage *gicon_get_icon_for_file_speed (file_entry *fe,
					      gboolean do_quick);

#endif
