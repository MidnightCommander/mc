gmc_helpfigdir = $(datadir)/gnome/help/gmc/C/figs

gmc_helpfig_DATA = $(DISTFIGS)

DISTFIGS = \
	fileprops.png \
	gccmime.png \
	gmc1.png \
	gmc_move.png \
	gmcprops1.png \
	gmcprops2.png \
	gmcprops3.png \
	gmcprops4.png \
	gmcprops5.png \
	gmcprops6.png \
	open_with.png \
	rubberband.png

EXTRA_DIST = $(DISTFIGS)

CLEANFILES = *.eps

SUFFIXES = .png .eps

CONVERT = convert
CONVERT_FLAGS = -geometry 50%x50%

.png.eps:
	$(CONVERT) $(CONVERT_FLAGS) $< $@

eps:
	for fig in $(DISTFIGS); do \
	  $(MAKE) `echo $$fig | sed 's/\.png$$/.eps/'` || exit 1; \
	done
