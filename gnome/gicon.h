#ifndef GNOME_GICON_H
#define GNOME_GICON_H

GdkImlibImage *gicon_get_by_filename            (char *fname);
GdkImlibImage *gicon_stock_load                 (char *basename);
GdkImlibImage *gicon_get_icon_for_file          (char *directory, file_entry *fe);
GdkImlibImage *gicon_get_icon_for_file_speed    (char *directory, file_entry *fe, gboolean do_quick);

char *gicon_image_to_name (GdkImlibImage *image);

#endif
