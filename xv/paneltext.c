/* Paneltext XView package.
   Copyright (C) 1995 Jakub Jelinek.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <xview/xview.h>
#include <xview/panel.h>
#include <xview_private/draw_impl.h>
#include <xview_private/panel_impl.h>
#include "paneltext.h"
#include "paneltext_impl.h"

Pkg_private int paneltext_init();
Pkg_private Xv_opaque paneltext_set_avlist();
Pkg_private Xv_opaque paneltext_get_attr();
Pkg_private int paneltext_destroy();
static void paneltext_accept_key(Paneltext paneltext, Event * event);

Xv_pkg paneltext_pkg =
{
    "Paneltext",
    ATTR_PANELTEXT,
    sizeof(Paneltext_struct),
    &xv_panel_text_pkg,
    paneltext_init,
    paneltext_set_avlist,
    paneltext_get_attr,
    paneltext_destroy,
    NULL
};

Pkg_private int paneltext_init(Xv_opaque parent, Paneltext paneltext, 
    Attr_avlist avlist)
{
    Paneltext_info *tinfo;
    Paneltext_struct *paneltext_object;
    Text_info *dp = TEXT_PRIVATE(paneltext);
    Item_info *ip = ITEM_PRIVATE(paneltext);

    tinfo = xv_alloc(Paneltext_info);

    tinfo->notify = (void (*)()) NULL;

    paneltext_object = (Paneltext_struct *) paneltext;
    paneltext_object->private_data = (Xv_opaque) tinfo;

    tinfo->accept_key = ip->ops.panel_op_accept_key;
    ip->ops.panel_op_accept_key = (void (*)()) paneltext_accept_key;

    return XV_OK;
}

Pkg_private Xv_opaque paneltext_set_avlist(Paneltext paneltext, 
    Attr_avlist avlist)
{
    register Paneltext_attr attr;
    register Paneltext_info *tinfo = PANELTEXT_PRIVATE(paneltext);

    if (*avlist != XV_END_CREATE) {
	Xv_opaque set_result;
	set_result =
	    xv_super_set_avlist(paneltext, &paneltext_pkg, avlist);
	if (set_result != XV_OK) {
	    return set_result;
	}
    }
    while ((attr = (Paneltext_attr) * avlist++))
	switch (attr) {

	case PANELTEXT_NOTIFY:
	    ATTR_CONSUME(*(avlist - 1));
	    tinfo->notify = (void (*)()) *avlist++;
	    break;

	case XV_END_CREATE:
	    break;

	default:
	    avlist = attr_skip(attr, avlist);

	}

    return XV_SET_DONE;
}

Pkg_private Xv_opaque paneltext_get_attr(Paneltext paneltext, int *status, 
    Attr_attribute which_attr, Attr_avlist avlist)
{
    Paneltext_info *tinfo = PANELTEXT_PRIVATE(paneltext);
    Text_info *dp = TEXT_PRIVATE(paneltext);

    switch (which_attr) {

    case PANELTEXT_NOTIFY:
	return (Xv_opaque) tinfo->notify;

    case PANELTEXT_CARETPOS:
	return (Xv_opaque) dp->caret_position;

    default:
	*status = XV_ERROR;
	return (Xv_opaque) 0;
    }
}

Pkg_private int paneltext_destroy(Paneltext paneltext, Destroy_status status)
{
    Paneltext_info *tinfo = PANELTEXT_PRIVATE(paneltext);

    if ((status == DESTROY_CHECKING) || (status == DESTROY_SAVE_YOURSELF))
	return XV_OK;

    free(tinfo);
    return XV_OK;
}

static void paneltext_accept_key(Paneltext paneltext, Event * event)
{
    Paneltext_info *tinfo = PANELTEXT_PRIVATE(paneltext);
    Text_info *dp = TEXT_PRIVATE(paneltext);

    if (tinfo->accept_key != NULL)
	(*tinfo->accept_key) (paneltext, event);
    if (tinfo->notify != NULL)
	(*tinfo->notify) (paneltext, event, dp->value, dp->caret_position);
}
