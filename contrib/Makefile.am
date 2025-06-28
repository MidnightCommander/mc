
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

mc.%: $(top_builddir)/config.status $(srcdir)/mc.%.in
	$(SED) "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.$*.in > $@

mc-wrapper.%: $(top_builddir)/config.status $(srcdir)/mc-wrapper.%.in
	$(SED) "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.$*.in > $@
