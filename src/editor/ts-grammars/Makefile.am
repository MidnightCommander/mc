# Makefile.am for tree-sitter grammar sources.
# Compiles selected grammar sources into a static convenience library (static
# mode) or per-grammar shared modules (shared mode).
#
# We use all-local/clean-local/install hooks instead of automake's _LTLIBRARIES
# because the source list is determined at configure time and automake does not
# support configure substitutions in _SOURCES variables.

EXTRA_DIST = \
	download-grammars.sh \
	ts-grammar-registry.h \
	tree_sitter/parser.h \
	tree_sitter/alloc.h \
	tree_sitter/array.h \
	tree_sitter/ts_assert.h

# Variables substituted by configure
TREE_SITTER_GRAMMAR_SOURCES = @TREE_SITTER_GRAMMAR_SOURCES@
TREE_SITTER_GRAMMAR_OBJECTS = @TREE_SITTER_GRAMMAR_OBJECTS@
TREE_SITTER_GRAMMARS = @TREE_SITTER_GRAMMARS@
TREE_SITTER_SHARED_LIBS = @TREE_SITTER_SHARED_LIBS@
TREE_SITTER_BUILD_TARGET = @TREE_SITTER_BUILD_TARGET@
TREE_SITTER_BUILD_MODE = @TREE_SITTER_BUILD_MODE@
TREE_SITTER_GRAMMAR_LIBDIR = @TREE_SITTER_GRAMMAR_LIBDIR@

TS_GRAMMAR_DIRS = $(TREE_SITTER_GRAMMARS)
OBJECTS = $(TREE_SITTER_GRAMMAR_OBJECTS)

AM_CPPFLAGS = -I$(top_srcdir) -I$(srcdir) $(TREE_SITTER_CFLAGS)
AM_CFLAGS = -w

# ============================================================================
# Build
# ============================================================================

all-local: $(TREE_SITTER_BUILD_TARGET)

# Static mode: single convenience library
libtsgrammars.la: check-grammar-sources $(OBJECTS)
	$(LIBTOOL) --tag=CXX --mode=link $(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

# Shared mode: one loadable module per grammar
shared-modules: check-grammar-sources $(OBJECTS)
	@for lang in $(TS_GRAMMAR_DIRS); do \
		objs="$$lang/parser.lo"; \
		linker="$(CC)"; tag="CC"; \
		if test -f "$$lang/scanner.lo"; then \
			objs="$$objs $$lang/scanner.lo"; \
		fi; \
		if test -f "$(srcdir)/$$lang/scanner.cc"; then \
			linker="$(CXX)"; tag="CXX"; \
		fi; \
		echo "  LINK     $$lang.so"; \
		$(LIBTOOL) --tag=$$tag --mode=link $$linker $(LDFLAGS) \
			-module -avoid-version -rpath $(TREE_SITTER_GRAMMAR_LIBDIR) \
			$(TREE_SITTER_LIBS) -o $$lang.la $$objs || exit 1; \
	done

# Pattern rules for compiling grammar sources
SUFFIXES = .c .cc .lo

.c.lo:
	@dir=`dirname $@`; test -d "$$dir" || mkdir -p "$$dir"
	$(LIBTOOL) --tag=CC --mode=compile $(CC) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c -o $@ $<

.cc.lo:
	@dir=`dirname $@`; test -d "$$dir" || mkdir -p "$$dir"
	$(LIBTOOL) --tag=CXX --mode=compile $(CXX) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CXXFLAGS) -c -o $@ $<

# ============================================================================
# Install
# ============================================================================

install-exec-local: install-exec-@TREE_SITTER_BUILD_MODE@

install-exec-static:
	@true

install-exec-shared:
	$(INSTALL) -d $(DESTDIR)$(TREE_SITTER_GRAMMAR_LIBDIR)
	@for lang in $(TS_GRAMMAR_DIRS); do \
		$(INSTALL) -m 755 .libs/$$lang.so $(DESTDIR)$(TREE_SITTER_GRAMMAR_LIBDIR)/$$lang.so; \
	done

# ============================================================================
# Clean
# ============================================================================

clean-local:
	@for lang in $(TS_GRAMMAR_DIRS); do \
		rm -f "$$lang"/*.lo "$$lang"/*.o; \
		rm -rf "$$lang/.libs"; \
	done
	rm -f libtsgrammars.la
	rm -f *.la
	rm -rf .libs

distclean-local:
	@for d in "$(srcdir)"/*/; do \
		case "$$d" in */tree_sitter/) ;; *) rm -rf "$$d" ;; esac; \
	done

# ============================================================================
# Grammar download
# ============================================================================

.PHONY: download-grammars shared-modules check-grammar-sources

download-grammars:
	@if test -n "$(TREE_SITTER_GRAMMARS_DIR)"; then \
		echo "Copying grammar sources from $(TREE_SITTER_GRAMMARS_DIR)..."; \
		for lang in $(TS_GRAMMAR_DIRS); do \
			if test -d "$(TREE_SITTER_GRAMMARS_DIR)/$$lang"; then \
				mkdir -p "$(srcdir)/$$lang"; \
				cp -a "$(TREE_SITTER_GRAMMARS_DIR)/$$lang"/* "$(srcdir)/$$lang/"; \
			else \
				echo "WARNING: $$lang not found in $(TREE_SITTER_GRAMMARS_DIR)"; \
			fi; \
		done; \
		echo "Done."; \
	else \
		$(SHELL) "$(srcdir)/download-grammars.sh" $(TS_GRAMMAR_DIRS); \
	fi

# Check that grammar sources exist before compiling.
check-grammar-sources:
	@missing=""; \
	for lang in $(TS_GRAMMAR_DIRS); do \
		if test ! -f "$(srcdir)/$$lang/parser.c"; then \
			missing="$$missing $$lang"; \
		fi; \
	done; \
	if test -n "$$missing"; then \
		echo ""; \
		echo "ERROR: Tree-sitter grammar sources not found for:$$missing"; \
		echo ""; \
		echo "Please run: ./configure --with-tree-sitter"; \
		echo "(configure auto-downloads missing grammar sources)"; \
		echo ""; \
		echo "Or manually: make -C src/editor/ts-grammars download-grammars"; \
		echo ""; \
		exit 1; \
	fi
