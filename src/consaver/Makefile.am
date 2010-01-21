pkglibexecdir =		$(libexecdir)/@PACKAGE@

pkglibexec_PROGRAMS = cons.saver

cons_saver_SOURCES = cons.saver.c

cons_saver_CFLAGS= -I$(top_srcdir) $(GLIB_CFLAGS) $(PCRE_CFLAGS)

cons_saver_LDADD = $(GLIB_LIBS) $(PCRE_LIBS)