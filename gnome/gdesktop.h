#define MC_LIB_DESKTOP "mc.desktop"

/* gtrans.c */
GtkWidget *create_transparent_text_window (char *file, char *text, int extra_events);
GtkWidget *make_transparent_window (char *file);

/* gdesktop.c */
void drop_on_panel (int requestor_window_id, char *dest);
