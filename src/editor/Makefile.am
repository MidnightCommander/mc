EXTRA_DIST =

if USE_EDIT
noinst_LTLIBRARIES = libedit.la
else
noinst_LTLIBRARIES =
endif

libedit_la_SOURCES = \
	bookmark.c edit.c editcmd.c editwidget.c editwidget.h \
	editdraw.c editkeys.c \
	editmenu.c editoptions.c edit-impl.h edit.h \
	syntax.c wordproc.c \
	choosesyntax.c etags.c etags.h editcmd_dialogs.c editcmd_dialogs.h

if USE_ASPELL
if HAVE_GMODULE
libedit_la_SOURCES += \
	spell.c spell.h \
	spell_dialogs.c spell_dialogs.h
endif
endif

libedit_la_CFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir) $(PCRE_CFLAGS)
