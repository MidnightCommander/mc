typedef struct _GCustomLayout GCustomLayout;

GCustomLayout *custom_layout_create_page (GnomePropertyBox *prop_box,
					  WPanel           *panel);
void           custom_layout_apply       (GCustomLayout    *layout);

