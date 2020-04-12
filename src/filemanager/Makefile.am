
noinst_LTLIBRARIES = libmcfilemanager.la

libmcfilemanager_la_SOURCES = \
	achown.c achown.h \
	boxes.c boxes.h \
	chmod.c chmod.h \
	chown.c chown.h \
	cmd.c cmd.h \
	command.c command.h \
	dir.c dir.h \
	ext.c ext.h \
	file.c file.h \
	filegui.c filegui.h \
	filenot.c filenot.h \
	fileopctx.c fileopctx.h \
	find.c find.h \
	hotlist.c hotlist.h \
	info.c info.h \
	ioblksize.h \
	layout.c layout.h \
	midnight.h midnight.c \
	mountlist.c mountlist.h \
	panelize.c panelize.h \
	panel.c panel.h \
	tree.c tree.h \
	treestore.c treestore.h

# Unmaintained, unsupported, etc
#	listmode.c listmode.h

AM_CPPFLAGS = -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CPPFLAGS)

if ENABLE_EXT2FS_ATTR
libmcfilemanager_la_SOURCES += \
	chattr.c chattr.h

AM_CPPFLAGS += @EXT2FS_CFLAGS@ @E2P_CFLAGS@
endif

libmcfilemanager_la_LIBADD = ../../lib/libmc.la
