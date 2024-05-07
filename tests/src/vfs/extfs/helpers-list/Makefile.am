PACKAGE_STRING = "/src/vfs/extfs/helpers-list"

SUBDIRS = misc

AM_CPPFLAGS = $(GLIB_CFLAGS) -I$(top_srcdir)

# This lets mc_parse_ls_l.c override MC's message() without the linker
# complaining about multiple definitions.
AM_LDFLAGS = @TESTS_LDFLAGS@

LIBS = $(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS)
endif

# Programs/scripts to build on 'make check'.
check_PROGRAMS = mc_parse_ls_l
check_SCRIPTS = run

# Tests to run on 'make check'
TESTS = run

# On 'make clean', delete 'run' as well.
CLEANFILES = run

mc_parse_ls_l_SOURCES = \
	mc_parse_ls_l.c

data_files_to_distribute = \
	data/hp48+.README \
	data/hp48+.args \
	data/hp48+.input \
	data/hp48+.output \
	data/iso9660.xorriso.env_vars \
	data/iso9660.xorriso.input \
	data/iso9660.xorriso.output \
	data/iso9660.xorriso.README \
	data/lslR.1.spaces.args \
	data/lslR.1.spaces.input \
	data/lslR.1.spaces.output \
	data/lslR.2.spaces-iso.args \
	data/lslR.2.spaces-iso.input \
	data/lslR.2.spaces-iso.output \
	data/lslR.3.spaces-iso-noslash.args \
	data/lslR.3.spaces-iso-noslash.input \
	data/lslR.3.spaces-iso-noslash.output \
	data/lslR.README \
	data/rpm.README \
	data/rpm.custom.env_vars \
	data/rpm.custom.input \
	data/rpm.custom.output \
	data/rpm.glib.env_vars \
	data/rpm.glib.input \
	data/rpm.glib.output \
	data/rpm.rewrite.sh \
	data/u7z.README \
	data/u7z.complex.env_vars \
	data/u7z.complex.input \
	data/u7z.complex.output \
	data/u7z.missing-date.env_vars \
	data/u7z.missing-date.input \
	data/u7z.missing-date.output \
	data/u7z.missing-size-and-date.env_vars \
	data/u7z.missing-size-and-date.input \
	data/u7z.missing-size-and-date.output \
	data/u7z.simple.input \
	data/u7z.simple.output \
	data/uace.README \
	data/uace.input \
	data/uace.output \
	data/uarc.README \
	data/uarc.input \
	data/uarc.output \
	data/urar.README \
	data/urar.v4,v3.env_vars \
	data/urar.v4,v3.input \
	data/urar.v4,v3.output \
	data/urar.v6,v5.env_vars \
	data/urar.v6,v5.input \
	data/urar.v6,v5.output \
	data/uzip.README \
	data/uzip.with-zipinfo.env_vars \
	data/uzip.with-zipinfo.input \
	data/uzip.with-zipinfo.output \
	data/uzip.without-zipinfo--mdy.env_vars \
	data/uzip.without-zipinfo--mdy.input \
	data/uzip.without-zipinfo--mdy.output \
	data/uzip.without-zipinfo--ymd.env_vars \
	data/uzip.without-zipinfo--ymd.input \
	data/uzip.without-zipinfo--ymd.output \
	data/uzoo.README \
	data/uzoo.input \
	data/uzoo.output \
	data/dummy

EXTRA_DIST = mc_xcat test_all $(data_files_to_distribute)

run:
	@echo '#!/bin/sh' > $@
	@echo >> $@
	@echo '# This script is an easy way to launch the "test_all" script' >> $@
	@echo '# with all the required arguments.' >> $@
	@echo '#' >> $@
	@echo '# Run this script with "--help" to learn more.' >> $@
	@echo >> $@
	@echo '# Where to find mc_parse_ls_l and mc_xcat, respectively.' >> $@
	@echo 'PATH="$(abs_builddir):$(abs_srcdir):$$PATH"' >> $@
	@echo >> $@
# The 'abs_' isn't mandatory. It lets you move this script out of the build tree.
	@echo '"$(abs_srcdir)"/test_all "$$@" \' >> $@
	@echo '  --data-dir "$(abs_srcdir)/data" \' >> $@
	@echo '  --data-build-dir "$(abs_builddir)/data" \' >> $@
# Before installation, some helpers are in the build tree, some in the src tree.
	@echo '  --helpers-dir "$(abs_top_builddir)/src/vfs/extfs/helpers" \' >> $@
	@echo '  --helpers-dir "$(abs_top_srcdir)/src/vfs/extfs/helpers"' >> $@
	@chmod +x $@
# (We can alternatively create run from a run.in template
# with 'AC_CONFIG_FILES[run, chmod +x run]'.)

# Whenever we change the recipe above, we need to regenerate the 'run' script:
run: Makefile

#
# Documentation
#

doc: README.html

# (Thanks to VPATH we don't need to write "$(srcdir)/README". doc/hlp/Makefile.am needlessly does this.)
README.html: README
	pandoc --include-in-header=$(srcdir)/README.css.inc -N --old-dashes --toc --toc-depth=4 --standalone -o '$@' '$(srcdir)/README'

EXTRA_DIST += README.css.inc
CLEANFILES += README.html
