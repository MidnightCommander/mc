gmc_helpdir = $(datadir)/gnome/help/gmc/C
gmc_helpfigdir = $(datadir)/gnome/help/gmc/C/figs

DISTFIGS = \
	figs/fileprops.png \
	figs/gccmime.png \
	figs/gmc1.png \
	figs/gmc_move.png \
	figs/gmcprops1.png \
	figs/gmcprops2.png \
	figs/gmcprops3.png \
	figs/gmcprops4.png \
	figs/gmcprops5.png \
	figs/gmcprops6.png \
	figs/open_with.png \
	figs/rubberband.png

gmc_help_DATA = index.html topic.dat
gmc_helpfig_DATA = $(DISTFIGS)

SGML_FILES = gmc.sgml

EXTRA_DIST = topic.dat $(SGML_FILES) $(DISTFIGS)

$(srcdir)/index.html: @MAINT@ $(SGML_FILES)
@MAINT@	db2html $(srcdir)/gmc.sgml && \
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

# "make distcheck" (automake-1.4) requires that *.dvi are created in the
# build directory, since the source directory is read-only
# However, db2dvi (docbook-3.1) doesn't allow redirecting the output. Sigh.
# For dvi to be useful you also need to convert *.png to *.eps
gmc.dvi: $(srcdir)/gmc.sgml
	if test $(srcdir) != .; then \
	  cp $(srcdir)/gmc.sgml gmc.sgml; \
	fi
	db2dvi gmc.sgml

gmc.ps: $(srcdir)/gmc.sgml
	db2ps gmc.sgml

gmc.rtf: $(srcdir)/gmc.sgml	
	db2rtf gmc.sgml
