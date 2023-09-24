shelldir = $(libexecdir)/@PACKAGE@/shell

# Files to install and distribute other than shell scripts
SHELL_MISC  = README.shell

# Install and distribute SHELL helper scripts w/o shebang & executable bit as data
shell_DATA = $(SHELL_MISC) ls mkdir fexists unlink chown chmod rmdir ln mv hardlink get send append info utime
shellconfdir = $(sysconfdir)/@PACKAGE@

EXTRA_DIST = $(shell_DATA)
