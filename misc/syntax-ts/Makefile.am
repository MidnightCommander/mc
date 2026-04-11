CONFIGFILES = \
	extensions \
	filenames \
	shebangs \
	display-names \
	symbols \
	wrappers \
	colors.ini

if USE_INTERNAL_EDIT
syntaxtsdir =		$(pkgdatadir)/syntax-ts
dist_syntaxts_DATA =	$(CONFIGFILES)

queriesdir =		$(pkgdatadir)/syntax-ts/queries

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(queriesdir)
	for f in $(srcdir)/queries/*-highlights.scm $(srcdir)/queries/*-injections.scm; do \
		test -f "$$f" && $(INSTALL_DATA) "$$f" $(DESTDIR)$(queriesdir)/; \
	done; true

uninstall-local:
	rm -f $(DESTDIR)$(queriesdir)/*-highlights.scm
	rm -f $(DESTDIR)$(queriesdir)/*-injections.scm
	-rmdir $(DESTDIR)$(queriesdir)
endif

EXTRA_DIST =		$(CONFIGFILES) \
			queries
