INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(CAPPLET_CFLAGS)

bin_PROGRAMS = mime-type-capplet

mime_type_capplet_SOURCES = mime-type-capplet.c\
	mime-data.h mime-data.c edit-window.c\
	edit-window.h mime-info.c mime-info.h \
	new-mime-window.h new-mime-window.c

mime_type_capplet_LDADD = $(CAPPLET_LIBS)

EXTRA_DIST = \
	mime-type.desktop


sysdir = $(datadir)/control-center
sys_DATA = mime-type.desktop 

install-data-local:
	$(INSTALL_DATA) $(srcdir)/mime-type.desktop $(DESTDIR)$(datadir)/gnome/apps/Settings/mime-type.desktop
