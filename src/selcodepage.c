#include <config.h>

#ifdef HAVE_CHARSET
#include <stdlib.h>
#include <stdio.h>
#include "i18n.h"
#include "dlg.h"
#include "dialog.h"
#include "widget.h"
#include "wtools.h"
#include "charsets.h"
#include "selcodepage.h"
#include "main.h"

#define ENTRY_LEN 35

/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int source_codepage = -1;
int display_codepage = -1;

static unsigned char get_hotkey( int n )
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

int select_charset( int current_charset, int seldisplay )
{
    int i, menu_lines = n_codepages + 1;
 
    /* Create listbox */
    Listbox* listbox =
	create_listbox_window ( ENTRY_LEN + 2, menu_lines,
				_(" Choose input codepage "),
				"[Codepages Translation]");

    if (!seldisplay)
	LISTBOX_APPEND_TEXT( listbox, '-', _("-  < No translation >"), NULL );

    /* insert all the items found */
    for (i = 0; i < n_codepages; i++) {
	struct codepage_desc cpdesc = codepages[i];
	char buffer[255];
	sprintf( buffer, "%c  %s", get_hotkey(i), cpdesc.name );
	LISTBOX_APPEND_TEXT( listbox, get_hotkey(i), buffer, NULL );
    }
    if (seldisplay) {
	char buffer[255];
	sprintf( buffer, "%c  %s", get_hotkey(n_codepages), _("Other 8 bit"));
	LISTBOX_APPEND_TEXT( listbox, get_hotkey(n_codepages), buffer, NULL );
    }
	
    /* Select the default entry */
    i = (seldisplay)
	?
	( (current_charset < 0) ? n_codepages : current_charset )
	:
	( current_charset + 1 );

    listbox_select_by_number( listbox->list, i );

    i = run_listbox( listbox );

    return (seldisplay) ? ( (i >= n_codepages) ? -1 : i )
			: ( i - 1 );
}

/* Helper functions for codepages support */


int do_select_codepage(void)
{
#ifndef HAVE_ICONV

    message( 1, _(" Warning "),
	    _("Midnight Commander was compiled without iconv support,\n"
	     "so charsets recoding feature is not available!" ));
    return -1;

#else

    char *errmsg;

    if (display_codepage > 0) {
	source_codepage = select_charset( source_codepage, 0 );
	errmsg = init_translation_table( source_codepage, display_codepage );
	if (errmsg) {
	    message( 1, MSG_ERROR, "%s", errmsg );
	    return -1;
	}
    } else {
	message( 1, _(" Warning "),
	       _("To use this feature select your codepage in\n"
		 "Setup / Display Bits dialog!\n"
		 "Do not forget to save options." ));
	return -1;
    }
    return 0;

#endif /* HAVE_ICONV */
}

#endif /* HAVE_CHARSET */
