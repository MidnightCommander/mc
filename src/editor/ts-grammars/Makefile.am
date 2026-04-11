# Makefile.am for tree-sitter grammar libraries.
# In static mode, merges pre-built .a files into a convenience library.
# In shared mode, grammar .so files are loaded at runtime (nothing to build).

EXTRA_DIST = \
	ts-grammar-registry.h

# Variables substituted by configure
TREE_SITTER_GRAMMAR_ARCHIVES = @TREE_SITTER_GRAMMAR_ARCHIVES@
TREE_SITTER_GRAMMARS = @TREE_SITTER_GRAMMARS@
TREE_SITTER_BUILD_TARGET = @TREE_SITTER_BUILD_TARGET@
TREE_SITTER_BUILD_MODE = @TREE_SITTER_BUILD_MODE@
TREE_SITTER_GRAMMAR_LIBDIR = @TREE_SITTER_GRAMMAR_LIBDIR@

# ============================================================================
# Build
# ============================================================================

all-local: $(TREE_SITTER_BUILD_TARGET)

# Static mode: merge pre-built .a files into a single static library.
# Extract .o files from each grammar archive, then combine into one .a.
libtsgrammars.a:
	@rm -rf _objs && mkdir -p _objs; \
	for archive in $(TREE_SITTER_GRAMMAR_ARCHIVES); do \
		lang=`echo $$archive | sed 's|/.*||'`; \
		mkdir -p _objs/$$lang; \
		(cd _objs/$$lang && $(AR) x $(abs_srcdir)/$$archive); \
	done; \
	all_objs=""; \
	for archive in $(TREE_SITTER_GRAMMAR_ARCHIVES); do \
		lang=`echo $$archive | sed 's|/.*||'`; \
		for o in _objs/$$lang/*.o; do \
			test -f "$$o" && all_objs="$$all_objs $$o"; \
		done; \
	done; \
	echo "  AR       libtsgrammars.a"; \
	$(AR) cr $@ $$all_objs; \
	$(RANLIB) $@; \
	rm -rf _objs

# ============================================================================
# Install
# ============================================================================

install-exec-local: install-exec-@TREE_SITTER_BUILD_MODE@

install-exec-static:
	@true

install-exec-shared:
	@true

# ============================================================================
# Clean
# ============================================================================

clean-local:
	rm -f libtsgrammars.a
	rm -rf _objs
