#define MC_LIB_DESKTOP "mc.desktop"

/* gtrans.c */
GtkWidget *create_transparent_text_window (char *file, char *text, int extra_events);
GtkWidget *make_transparent_window (char *file);

/* gdesktop.c */
void drop_on_directory (GdkEventDropDataAvailable *event, char *dest, int force_manually);
void artificial_drag_start (GdkWindow *source_window, int x, int y);
