SUBDIRS = helpers
DIST_SUBDIRS = helpers

AM_CPPFLAGS = \
	-DLIBEXECDIR=\""$(pkglibexecdir)/"\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)

noinst_LTLIBRARIES = libvfs-shell.la

libvfs_shell_la_SOURCES = \
	shell.c \
	shell.h

shell.c:	shelldef.h

CLEANFILES = shelldef.h

SHELL_HELPERS = \
	append \
	chmod \
	chown \
	fexists \
	get \
	hardlink \
	info \
	ln \
	ls \
	mkdir \
	mv \
	rmdir \
	send \
	unlink \
	utime

shelldef.h: $(SHELL_HELPERS:%=$(srcdir)/helpers/%)
	for _src in $(SHELL_HELPERS); do \
		_upper="$$(printf '%s' "$$_src" | $(SED) -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' -e 's/FEXISTS/EXISTS/')"; \
		printf '#define VFS_SHELL_%s_DEF_CONTENT ( \\\n' "$$_upper"; \
		$(SED) \
			-e 's/[\\"]/\\&/g' \
			-e 's/.*/    "&\\n" \\/' \
			"$(srcdir)/helpers/$$_src"; \
		printf ')\n\n'; \
	done > $@
