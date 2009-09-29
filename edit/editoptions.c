/* editor options dialog box

   Copyright (C) 1996, 1997, 1998, 2001, 2002, 2003, 2004, 2005, 2007
   Free Software Foundation, Inc.

   Authors: 1996, 1997 Paul Sheer

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/** \file
 *  \brief Source: editor options dialog box
 *  \author Paul Sheer
 *  \date 1996, 1997
 */

#include <config.h>

#include <stdlib.h> /* atoi(), NULL */

#include "../src/global.h"

#include "edit-impl.h"
#include "../src/dialog.h"	/* B_CANCEL */
#include "../src/wtools.h"	/* QuickDialog */

#define OPT_DLG_H 21
#define OPT_DLG_W 72

static const char *key_emu_str[] =
{
    N_("Intuitive"),
    N_("Emacs"),
    N_("User-defined"),
    NULL
};

static const char *wrap_str[] =
{
    N_("None"),
    N_("Dynamic paragraphing"),
    N_("Type writer wrap"),
    NULL
};

static void
i18n_translate_array (const char *array[])
{
    while (*array != NULL) {
	*array = _(*array);
        array++;
    }
}

void
edit_options_dialog (void)
{
    char wrap_length[16], tab_spacing[16], *p, *q;
    int wrap_mode = 0;
    int old_syntax_hl;
    int tedit_key_emulation = edit_key_emulation;

    QuickWidget quick_widgets[] =
    {
	/*  0 */ QUICK_BUTTON (6, 10, OPT_DLG_H - 3, OPT_DLG_H, N_("&Cancel"), B_CANCEL, NULL),
	/*  1 */ QUICK_BUTTON (2, 10, OPT_DLG_H - 3, OPT_DLG_H, N_("&OK"),     B_ENTER,  NULL),
	/*  2 */ QUICK_LABEL (OPT_DLG_W / 2, OPT_DLG_W, OPT_DLG_H - 6, OPT_DLG_H, N_("Word wrap line length: ")),
	/*  3 */ QUICK_INPUT (OPT_DLG_W / 2 + 24, OPT_DLG_W, OPT_DLG_H - 6, OPT_DLG_H,
				wrap_length, OPT_DLG_W / 2 - 4 - 24, 0, "edit-word-wrap", &p),
	/*  4 */ QUICK_LABEL (OPT_DLG_W / 2, OPT_DLG_W, OPT_DLG_H - 7, OPT_DLG_H, N_("Tab spacing: ")),
	/*  5 */ QUICK_INPUT (OPT_DLG_W / 2 + 24, OPT_DLG_W, OPT_DLG_H - 7, OPT_DLG_H,
				tab_spacing, OPT_DLG_W / 2 - 4 - 24, 0, "edit-tab-spacing", &q),
	/*  6 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 8, OPT_DLG_H,
				N_("Cursor beyond end of line"), &option_cursor_beyond_eol),
	/*  7 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H -  9, OPT_DLG_H,
				N_("Pers&istent selection"), &option_persistent_selections),
	/*  8 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 10, OPT_DLG_H,
				N_("Synta&x highlighting"), &option_syntax_highlighting),
	/*  9 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 11, OPT_DLG_H,
				N_("Visible tabs"), &visible_tabs),
	/* 10 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 12, OPT_DLG_H,
				N_("Visible trailing spaces"), &visible_tws),
	/* 11 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 13, OPT_DLG_H,
				N_("Save file &position"), &option_save_position),
	/* 12 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 14, OPT_DLG_H,
				N_("Confir&m before saving"), &edit_confirm_save),
	/* 13 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 15, OPT_DLG_H,
				N_("Fill tabs with &spaces"), &option_fill_tabs_with_spaces),
	/* 14 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 16, OPT_DLG_H,
				N_("&Return does autoindent"), &option_return_does_auto_indent),
	/* 15 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 17, OPT_DLG_H,
				N_("&Backspace through tabs"), &option_backspace_through_tabs),
	/* 16 */ QUICK_CHECKBOX (OPT_DLG_W / 2 + 1, OPT_DLG_W, OPT_DLG_H - 18, OPT_DLG_H,
				 N_("&Fake half tabs"), &option_fake_half_tabs),
	/* 17 */ QUICK_RADIO (5, OPT_DLG_W, OPT_DLG_H - 11, OPT_DLG_H, 3, wrap_str, &wrap_mode),
	/* 18 */ QUICK_LABEL (4, OPT_DLG_W, OPT_DLG_H - 12, OPT_DLG_H, N_("Wrap mode")),
	/* 19 */ QUICK_RADIO (5, OPT_DLG_W, OPT_DLG_H - 17, OPT_DLG_H, 3, key_emu_str, &tedit_key_emulation),
	/* 10 */ QUICK_LABEL (4, OPT_DLG_W, OPT_DLG_H - 18, OPT_DLG_H, N_("Key emulation")),
	QUICK_END
    };

    QuickDialog Quick_options =
    {
	 OPT_DLG_W, OPT_DLG_H, -1, -1, N_(" Editor options "),
	"[Editor options]", quick_widgets, FALSE
     };

#ifdef ENABLE_NLS
    static gboolean i18n_flag = FALSE;

    if (!i18n_flag) {
	i18n_translate_array (key_emu_str);
	i18n_translate_array (wrap_str);
	i18n_flag = TRUE;
    }
#endif

    g_snprintf (wrap_length, sizeof (wrap_length), "%d",
		option_word_wrap_line_length);
    g_snprintf (tab_spacing, sizeof (tab_spacing), "%d",
		option_tab_spacing);

    if (option_auto_para_formatting)
	wrap_mode = 1;
    else if (option_typewriter_wrap)
	wrap_mode = 2;
    else
	wrap_mode = 0;

    if (quick_dialog (&Quick_options) == B_CANCEL)
	return;

    old_syntax_hl = option_syntax_highlighting;

    if (p) {
	option_word_wrap_line_length = atoi (p);
	g_free (p);
    }
    if (q) {
	option_tab_spacing = atoi (q);
	if (option_tab_spacing <= 0)
	    option_tab_spacing = 8;
	g_free (q);
    }

    if (wrap_mode == 1) {
	option_auto_para_formatting = 1;
	option_typewriter_wrap = 0;
    } else if (wrap_mode == 2) {
	option_auto_para_formatting = 0;
	option_typewriter_wrap = 1;
    } else {
	option_auto_para_formatting = 0;
	option_typewriter_wrap = 0;
    }

    /* Reload menu if key emulation has changed */
    if (edit_key_emulation != tedit_key_emulation) {
	edit_key_emulation = tedit_key_emulation;
	edit_reload_menu ();
    }

    /* Load or unload syntax rules if the option has changed */
    if (option_syntax_highlighting != old_syntax_hl)
	edit_load_syntax (wedit, NULL, option_syntax_type);
}
