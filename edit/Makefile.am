AM_CFLAGS = $(GLIB_CFLAGS)

if USE_EDIT
noinst_LIBRARIES = libedit.a
else
noinst_LIBRARIES =
endif

libedit_a_SOURCES = edit.c editcmd.c editwidget.c editdraw.c \
	editmenu.c wordproc.c syntax.c editoptions.c bookmark.c \
	editcmddef.h edit.h edit-widget.h

EXTRASRC = edit_key_translator.c
EXTRADOCS = ChangeLog

EXTRA_DIST = $(EXTRASRC) $(EXTRADOCS)
