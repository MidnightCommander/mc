SUBDIRS = figs

gmc_helpdir = $(datadir)/gnome/help/gmc/C

gmc_help_DATA = index.html topic.dat

noinst_SCRIPTS = mkhtml

SGML_FILES = gmc.sgml

EXTRA_DIST = topic.dat $(SGML_FILES) $(noinst_SCRIPTS)

MOSTLYCLEANFILES = *.tex *.log *.aux
CLEANFILES = gmc.dvi gmc.ps gmc.pdf gmc.rtf
DISTCLEANFILES = gmc.sgm

$(srcdir)/index.html: @MAINT@ $(SGML_FILES)
@MAINT@	-cd $(srcdir) && $(SHELL) $(srcdir)/mkhtml

dist-hook: $(srcdir)/index.html
	cp $(srcdir)/*.html $(distdir)

install-data-local: $(srcdir)/index.html
	-for file in $(srcdir)/*.html; do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(gmc_helpdir) || exit 1; \
	done

uninstall-local: $(srcdir)/index.html
	rm -f $(DESTDIR)$(gmc_helpdir)/*.html

dvi-local: gmc.dvi

# "make distcheck" (automake-1.4a) requires that *.dvi are created in
# the build directory, since the source directory is read-only.
gmc.dvi: $(srcdir)/gmc.sgml
	(cd figs && $(MAKE) eps)
	db2dvi $(srcdir)/gmc.sgml

gmc.rtf: $(srcdir)/gmc.sgml
	db2rtf $(srcdir)/gmc.sgml

gmc.ps: gmc.dvi
	dvips gmc.dvi -o gmc.ps

gmc.pdf: gmc.dvi
	dvipdf gmc.dvi gmc.pdf
