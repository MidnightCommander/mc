PACKAGE_STRING = "/lib/skin"

AM_CPPFLAGS = \
	-DTEST_SHARE_DIR=\"$(abs_srcdir)/fixtures\" \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir) \
	@CHECK_CFLAGS@

LIBS = @CHECK_LIBS@ \
	$(top_builddir)/src/libinternal.la \
	$(top_builddir)/lib/libmc.la

if ENABLE_MCLIB
LIBS += $(GLIB_LIBS) \
	@E2P_LIBS@
endif

EXTRA_DIST = \
	fixtures/skins/dark.ini \
	fixtures/skins/julia256.ini \
	fixtures/skins/seasons-summer16M.ini

TESTS = \
	common__mc_skin_init

check_PROGRAMS = $(TESTS)

common__mc_skin_init_SOURCES = \
	common__mc_skin_init.c
