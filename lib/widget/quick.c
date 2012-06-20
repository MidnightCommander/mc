/*
   Widget based utility functions.

   Copyright (C) 1994, 1995, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
   2005, 2006, 2007, 2008, 2009, 2010, 2011
   The Free Software Foundation, Inc.

   Authors:
   Miguel de Icaza, 1994, 1995, 1996
   Radek Doulik, 1994, 1995
   Jakub Jelinek, 1995
   Andrej Borsenkow, 1995
   Andrew Borodin <aborodin@vmail.ru>, 2009, 2010

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file quick.c
 *  \brief Source: quick dialog engine
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>              /* fprintf() */

#include "lib/global.h"
#include "lib/util.h"           /* tilde_expand() */
#include "lib/widget.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
quick_dialog_skip (QuickDialog * qd, int nskip)
{
#ifdef ENABLE_NLS
#define I18N(x) (x = !qd->i18n && x != NULL && *x != '\0' ? _(x): x)
#else
#define I18N(x) (x = x)
#endif
    Dlg_head *dd;
    QuickWidget *qw;
    WInput *in;
    WRadio *r;
    int return_val;

    I18N (qd->title);

    if ((qd->xpos == -1) || (qd->ypos == -1))
        dd = create_dlg (TRUE, 0, 0, qd->ylen, qd->xlen,
                         dialog_colors, qd->callback, qd->mouse, qd->help, qd->title,
                         DLG_CENTER | DLG_TRYUP | DLG_REVERSE);
    else
        dd = create_dlg (TRUE, qd->ypos, qd->xpos, qd->ylen, qd->xlen,
                         dialog_colors, qd->callback, qd->mouse, qd->help, qd->title, DLG_REVERSE);

    for (qw = qd->widgets; qw->widget_type != quick_end; qw++)
    {
        int xpos;
        int ypos;

        xpos = (qd->xlen * qw->relative_x) / qw->x_divisions;
        ypos = (qd->ylen * qw->relative_y) / qw->y_divisions;

        switch (qw->widget_type)
        {
        case quick_checkbox:
            qw->widget = WIDGET (check_new (ypos, xpos, *qw->u.checkbox.state,
                                            I18N (qw->u.checkbox.text)));
            break;

        case quick_button:
            qw->widget = WIDGET (button_new (ypos, xpos, qw->u.button.action,
                                             qw->u.button.action == B_ENTER ?
                                                 DEFPUSH_BUTTON : NORMAL_BUTTON,
                                             I18N (qw->u.button.text), qw->u.button.callback));
            break;

        case quick_input:
            in = input_new (ypos, xpos, input_get_default_colors (),
                            qw->u.input.len, qw->u.input.text, qw->u.input.histname,
                            INPUT_COMPLETE_DEFAULT);
            in->is_password = (qw->u.input.flags == 1);
            if ((qw->u.input.flags & 2) != 0)
                in->completion_flags |= INPUT_COMPLETE_CD;
            if ((qw->u.input.flags & 4) != 0)
                in->strip_password = TRUE;
            qw->widget = WIDGET (in);
            *qw->u.input.result = NULL;
            break;

        case quick_label:
            qw->widget = WIDGET (label_new (ypos, xpos, I18N (qw->u.label.text)));
            break;

        case quick_groupbox:
            qw->widget = WIDGET (groupbox_new (ypos, xpos,
                                               qw->u.groupbox.height,
                                               qw->u.groupbox.width,
                                               I18N (qw->u.groupbox.title)));
            break;

        case quick_radio:
            {
                int i;
                char **items = NULL;

                /* create the copy of radio_items to avoid mwmory leak */
                items = g_new0 (char *, qw->u.radio.count + 1);

                if (!qd->i18n)
                    for (i = 0; i < qw->u.radio.count; i++)
                        items[i] = g_strdup (_(qw->u.radio.items[i]));
                else
                    for (i = 0; i < qw->u.radio.count; i++)
                        items[i] = g_strdup (qw->u.radio.items[i]);

                r = radio_new (ypos, xpos, qw->u.radio.count, (const char **) items);
                r->pos = r->sel = *qw->u.radio.value;
                qw->widget = WIDGET (r);
                g_strfreev (items);
                break;
            }

        default:
            qw->widget = NULL;
            fprintf (stderr, "QuickWidget: unknown widget type\n");
            break;
        }

        if (qw->widget != NULL)
        {
            qw->widget->options |= qw->options; /* FIXME: cannot reset flags, setup only */
            add_widget (dd, qw->widget);
        }
    }

    while (nskip-- != 0)
    {
        dd->current = g_list_next (dd->current);
        if (dd->current == NULL)
            dd->current = dd->widgets;
    }

    return_val = run_dlg (dd);

    /* Get the data if we found something interesting */
    if (return_val != B_CANCEL)
    {
        for (qw = qd->widgets; qw->widget_type != quick_end; qw++)
        {
            switch (qw->widget_type)
            {
            case quick_checkbox:
                *qw->u.checkbox.state = ((WCheck *) qw->widget)->state & C_BOOL;
                break;

            case quick_input:
                if ((qw->u.input.flags & 2) != 0)
                    *qw->u.input.result = tilde_expand (((WInput *) qw->widget)->buffer);
                else
                    *qw->u.input.result = g_strdup (((WInput *) qw->widget)->buffer);
                break;

            case quick_radio:
                *qw->u.radio.value = ((WRadio *) qw->widget)->sel;
                break;

            default:
                break;
            }
        }
    }

    destroy_dlg (dd);

    return return_val;
#undef I18N
}

/* --------------------------------------------------------------------------------------------- */
