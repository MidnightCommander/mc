## Process this file with automake to create Makefile.in.

SUBDIRS = dist

if CHARSET
LIBFILES_ADD = mc.charsets
else
LIBFILES_ADD =
endif

noinst_DATA = README.xterm mcserv.init mcserv.pamd

SCRIPTS_IN = mc.csh.in mc.sh.in mc-wrapper.csh.in mc-wrapper.sh.in
SCRIPTS_OUT = mc.csh mc.sh mc-wrapper.csh mc-wrapper.sh

pkglibexec_SCRIPTS = $(SCRIPTS_OUT)
pkglibexecdir =		$(libexecdir)/@PACKAGE@

cfgdir = $(sysconfdir)/@PACKAGE@
cfg_DATA = \
	$(LIBFILES_ADD)

CLEANFILES = $(SCRIPTS_OUT)

# Files processed by configure don't need to be here
EXTRA_DIST = \
	$(SCRIPTS_IN) \
	$(noinst_DATA) \
	$(cfg_DATA)

mc.csh: $(srcdir)/mc.csh.in
	sed "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.csh.in > mc.csh

mc.sh: $(srcdir)/mc.sh.in
	sed "s%@""pkglibexecdir@%$(pkglibexecdir)%" $(srcdir)/mc.sh.in > mc.sh

mc-wrapper.csh: $(srcdir)/mc-wrapper.csh.in
	sed "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.csh.in > mc-wrapper.csh

mc-wrapper.sh: $(srcdir)/mc-wrapper.sh.in
	sed "s%@""bindir@%$(bindir)%" $(srcdir)/mc-wrapper.sh.in > mc-wrapper.sh

