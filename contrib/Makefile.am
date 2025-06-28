
noinst_DATA = README.xterm

SCRIPTS_IN = \
	mc.csh.in \
	mc.fish.in \
	mc.sh.in \
	mc-wrapper.csh.in \
	mc-wrapper.fish.in \
	mc-wrapper.sh.in
SCRIPTS_OUT = \
	mc.csh \
	mc.fish \
	mc.sh \
	mc-wrapper.csh \
	mc-wrapper.fish \
	mc-wrapper.sh

pkglibexec_SCRIPTS = $(SCRIPTS_OUT)

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_ADD)

CLEANFILES = $(SCRIPTS_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(SCRIPTS_IN) \
	$(noinst_DATA)

mc.csh: $(top_builddir)/config.status $(srcdir)/mc.csh.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.csh.in > mc.csh

mc.fish: $(top_builddir)/config.status $(srcdir)/mc.fish.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.fish.in > mc.fish

mc.sh: $(top_builddir)/config.status $(srcdir)/mc.sh.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.sh.in > mc.sh

mc-wrapper.csh: $(top_builddir)/config.status $(srcdir)/mc-wrapper.csh.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.csh.in > mc-wrapper.csh

mc-wrapper.fish: $(top_builddir)/config.status $(srcdir)/mc-wrapper.fish.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.fish.in > mc-wrapper.fish

mc-wrapper.sh: $(top_builddir)/config.status $(srcdir)/mc-wrapper.sh.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.sh.in > mc-wrapper.sh

