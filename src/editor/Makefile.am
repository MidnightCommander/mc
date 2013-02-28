EXTRA_DIST =

if USE_EDIT
noinst_LTLIBRARIES = libedit.la
else
noinst_LTLIBRARIES =
endif

libedit_la_SOURCES = \
	bookmark.c \
	choosesyntax.c \
	edit.c edit.h \
	edit-impl.h \
	editcmd.c \
	editcmd_dialogs.c editcmd_dialogs.h \
	editdraw.c \
	editmenu.c \
	editoptions.c \
	editwidget.c editwidget.h \
	etags.c etags.h \
	syntax.c \
	wordproc.c

if USE_ASPELL
if HAVE_GMODULE
libedit_la_SOURCES += \
	spell.c spell.h \
	spell_dialogs.c spell_dialogs.h
endif
endif

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CPPFLAGS)
