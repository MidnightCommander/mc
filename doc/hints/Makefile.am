HINTFILE = mc.hint
HINTFILES_LANG = \
        mc.hint.cs mc.hint.es mc.hint.hu mc.hint.it mc.hint.nl \
        mc.hint.pl mc.hint.ru mc.hint.sr mc.hint.uk mc.hint.zh

hintdir = $(pkgdatadir)/hints

hint_DATA = $(HINTFILE)
if USE_NLS
hint_DATA += $(HINTFILES_LANG)
endif

# Files processed by configure don't need to be here
EXTRA_DIST = $(HINTFILE) $(HINTFILES_LANG)
