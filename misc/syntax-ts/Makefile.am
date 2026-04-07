CONFIGFILES = \
	extensions \
	filenames \
	shebangs \
	display-names \
	symbols \
	colors.ini

if USE_INTERNAL_EDIT
syntaxtsdir =		$(pkgdatadir)/syntax-ts
dist_syntaxts_DATA =	$(CONFIGFILES)

queriesdir =		$(pkgdatadir)/syntax-ts/queries
overridedir =		$(pkgdatadir)/syntax-ts/queries-override

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(queriesdir)
	$(MKDIR_P) $(DESTDIR)$(overridedir)
	for f in $(srcdir)/queries/*-highlights.scm $(srcdir)/queries/*-injections.scm; do \
		test -f "$$f" && $(INSTALL_DATA) "$$f" $(DESTDIR)$(queriesdir)/; \
	done; true
	for f in $(srcdir)/queries-override/*-highlights.scm $(srcdir)/queries-override/*-injections.scm; do \
		test -f "$$f" && $(INSTALL_DATA) "$$f" $(DESTDIR)$(overridedir)/; \
	done; true

uninstall-local:
	rm -f $(DESTDIR)$(queriesdir)/*-highlights.scm
	rm -f $(DESTDIR)$(queriesdir)/*-injections.scm
	rm -f $(DESTDIR)$(overridedir)/*-highlights.scm
	rm -f $(DESTDIR)$(overridedir)/*-injections.scm
	-rmdir $(DESTDIR)$(overridedir)
	-rmdir $(DESTDIR)$(queriesdir)
endif

EXTRA_DIST =		$(CONFIGFILES) \
			queries \
			queries-override

# Build an MC-ready grammar tarball from the shared download staging directory.
# Requires: ./scripts/ts-grammars-download.sh --shared to have been run first.
# Output: mc-ts-grammars-<platform>.tar.gz in the top build directory.
#
# The tarball layout:
#   mc-ts-grammars-<platform>/
#     lib/<lang>.so          -- grammar shared libraries (bare names)
#     queries/<lang>-highlights.scm
#     queries/<lang>-injections.scm  (where available)
#     queries-override/      -- MC-specific query overrides
dist-shared:
	@staging="$(top_srcdir)/ts-grammars-shared"; \
	if [ ! -d "$$staging" ]; then \
	    echo "Error: shared grammars not found. Run first:"; \
	    echo "  ./scripts/ts-grammars-download.sh --shared"; \
	    exit 1; \
	fi; \
	arch=$$(uname -m); \
	case "$$(uname -s)" in \
	    Linux)  platform="$${arch}-linux" ;; \
	    Darwin) case "$$arch" in arm64) platform="aarch64-macos" ;; *) platform="$${arch}-macos" ;; esac ;; \
	    MINGW*) platform="$${arch}-windows" ;; \
	    *)      platform="$${arch}-unknown" ;; \
	esac; \
	outdir="$(top_builddir)/mc-ts-grammars-$$platform"; \
	rm -rf "$$outdir"; \
	mkdir -p "$$outdir/lib" "$$outdir/queries" "$$outdir/queries-override"; \
	count=0; \
	for lang_dir in "$$staging"/*/; do \
	    lang=$$(basename "$$lang_dir"); \
	    for ext in so dylib dll; do \
	        for lib in "$$lang_dir"/$$lang.$$ext; do \
	            [ -f "$$lib" ] && cp "$$lib" "$$outdir/lib/"; \
	        done; \
	    done; \
	    count=$$((count + 1)); \
	done; \
	for f in $(srcdir)/queries/*-highlights.scm $(srcdir)/queries/*-injections.scm; do \
	    [ -f "$$f" ] && cp "$$f" "$$outdir/queries/"; \
	done; true; \
	for f in $(srcdir)/queries-override/*-highlights.scm $(srcdir)/queries-override/*-injections.scm; do \
	    [ -f "$$f" ] && cp "$$f" "$$outdir/queries-override/"; \
	done; true; \
	(cd "$(top_builddir)" && tar czf "mc-ts-grammars-$$platform.tar.gz" "mc-ts-grammars-$$platform"); \
	rm -rf "$$outdir"; \
	echo "Created: $(top_builddir)/mc-ts-grammars-$$platform.tar.gz ($$count grammars)"
