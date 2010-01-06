AM_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)

if USE_EDIT
noinst_LIBRARIES = libedit.a
else
noinst_LIBRARIES =
endif

libedit_a_SOURCES = \
	bookmark.c edit.c editcmd.c editwidget.c editdraw.c editkeys.c \
	editmenu.c editoptions.c edit-impl.h edit.h edit-widget.h \
	editlock.c editlock.h syntax.c wordproc.c \
	choosesyntax.c etags.c etags.h editcmd_dialogs.c editcmd_dialogs.h

EXTRA_DIST =
