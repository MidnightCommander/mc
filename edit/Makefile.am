AM_CFLAGS = $(GLIB_CFLAGS)

if USE_EDIT
noinst_LIBRARIES = libedit.a
else
noinst_LIBRARIES =
endif

libedit_a_SOURCES = \
	bookmark.c edit.c editcmd.c editwidget.c editdraw.c editkeys.c \
	editmenu.c editoptions.c editcmddef.h edit.h edit-widget.h \
	editlock.c editlock.h syntax.c wordproc.c

EXTRA_DIST = ChangeLog
