EXTRA_DIST =

if USE_INTERNAL_EDIT
noinst_LTLIBRARIES = libedit.la
else
noinst_LTLIBRARIES =
endif

libedit_la_SOURCES = \
	bookmark.c \
	edit-impl.h \
	edit.c edit.h \
	editcomplete.c editcomplete.h \
	editbuffer.c editbuffer.h \
	editcmd.c \
	editdraw.c \
	editmacros.c editmacros.h \
	editmenu.c \
	editoptions.c \
	editsearch.c editsearch.h \
	editwidget.c editwidget.h \
	etags.c etags.h \
	format.c \
	syntax.c

if USE_ASPELL
if HAVE_GMODULE
libedit_la_SOURCES += \
	spell.c spell.h
endif
endif

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)
