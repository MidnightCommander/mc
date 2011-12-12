fishdir = $(libexecdir)/@PACKAGE@/fish

# Files to install and distribute other than fish scripts
FISH_MISC  = README.fish

fish_DATA = $(FISH_MISC)
fish_SCRIPTS = ls mkdir fexists unlink chown chmod rmdir ln mv hardlink get send append info utime
fishconfdir = $(sysconfdir)/@PACKAGE@

EXTRA_DIST = $(FISH_MISC) $(fish_SCRIPTS)
