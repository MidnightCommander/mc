/* Custom layout preferences box for the Midnight Commander
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Owen Taylor <otaylor@redhat.com>
 */

typedef struct _GCustomLayout GCustomLayout;

GCustomLayout *custom_layout_create_page (GnomePropertyBox *prop_box,
					  WPanel           *panel);
void           custom_layout_apply       (GCustomLayout    *layout);

