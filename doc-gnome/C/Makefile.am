SUBDIRS = figs

gmc_helpdir = $(datadir)/gnome/help/gmc/C

gmc_help_DATA = index.html topic.dat

SGML_FILES = gmc.sgml

EXTRA_DIST = topic.dat $(SGML_FILES)

MOSTLYCLEANFILES = *.tex *.log *.aux
CLEANFILES = gmc.dvi gmc.ps gmc.pdf gmc.rtf
DISTCLEANFILES = gmc.sgm

$(srcdir)/index.html: @MAINT@ $(SGML_FILES)
@MAINT@	fullsrcdir=`cd $(srcdir) >/dev/null 2>&1 && pwd` && \
@MAINT@	db2html $$fullsrcdir/gmc.sgml && \
@MAINT@	  rm -f $(srcdir)/*.html $(srcdir)/*.css && \
@MAINT@	  mv gmc/*.html gmc/*.css $(srcdir)
@MAINT@	rm -rf gmc

dist-hook: $(srcdir)/index.html
	cp $(srcdir)/*.html $(srcdir)/*.css $(distdir)

install-data-local: $(srcdir)/index.html
	for file in $(srcdir)/*.html $(srcdir)/*.css; do \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(gmc_helpdir) || exit 1; \
	done

uninstall-local: $(srcdir)/index.html
	rm -f $(DESTDIR)$(gmc_helpdir)/*.html
	rm -f $(DESTDIR)$(gmc_helpdir)/*.css

dvi-local: gmc.dvi

# "make distcheck" (automake-1.4a) requires that *.dvi are created in
# the build directory, since the source directory is read-only.
# However, db2dvi (docbook-3.1) doesn't allow redirecting the output.
# So let's copy gmc.sgml to a temp directory. Now *.dvi goes to ".",
# but *.rtf remains in "tmp", so let's try to copy it from "tmp" -
# maybe future versions of docbook will be more consistent.
gmc.dvi: $(srcdir)/gmc.sgml
	mkdir tmpdvi
	cp $(srcdir)/gmc.sgml tmpdvi
	db2dvi tmpdvi/gmc.sgml
	if test -f tmpdvi/gmc.dvi; then \
	  mv -f tmpdvi/gmc.dvi gmc.dvi; \
	else :; fi
	rm -rf tmpdvi

gmc.rtf: $(srcdir)/gmc.sgml
	mkdir tmprtf
	cp $(srcdir)/gmc.sgml tmprtf
	db2rtf tmprtf/gmc.sgml
	if test -f tmprtf/gmc.rtf; then \
	  mv -f tmprtf/gmc.rtf gmc.rtf; \
	else :; fi
	rm -rf tmprtf

# FIXME: for gmc.ps and gmc.pdf we also need to convert *.png to *.eps
gmc.ps: gmc.dvi
	dvips gmc.dvi -o gmc.ps

gmc.pdf: gmc.dvi
	dvipdf gmc.dvi gmc.pdf
