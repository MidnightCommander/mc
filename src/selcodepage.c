#include <config.h>

#ifdef HAVE_CHARSET
#include <stdlib.h>
#include <stdio.h>
#include "dlg.h"
#include "widget.h"
#include "wtools.h"
#include "charsets.h"
#include "i18n.h"

#define ENTRY_LEN 35

/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int source_codepage = -1;
int display_codepage = -1;

unsigned char get_hotkey( int n )
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
#endif /* HAVE_CHARSET */
