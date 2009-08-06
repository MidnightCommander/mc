SYNTAXFILES =			\
	ada95.syntax		\
	awk.syntax		\
	aspx.syntax		\
	assembler.syntax	\
	c.syntax		\
	changelog.syntax	\
	debian-changelog.syntax	\
	debian-control.syntax	\
	debian-description.syntax \
	debian-sources-list.syntax \
	cs.syntax		\
	css.syntax		\
	cxx.syntax		\
	d.syntax		\
	diff.syntax		\
	dos.syntax		\
	ebuild.syntax		\
	eiffel.syntax		\
	erlang.syntax		\
	f90.syntax		\
	fortran.syntax		\
	haskell.syntax		\
	html.syntax		\
	idl.syntax		\
	java.syntax		\
	js.syntax		\
	latex.syntax		\
	lisp.syntax		\
	lsm.syntax		\
	lua.syntax		\
	nemerle.syntax		\
	m4.syntax		\
	mail.syntax		\
	makefile.syntax		\
	ml.syntax		\
	named.syntax    	\
	nroff.syntax		\
	octave.syntax		\
	pascal.syntax		\
	perl.syntax		\
	php.syntax		\
	po.syntax		\
	povray.syntax		\
	procmail.syntax		\
	python.syntax		\
	ruby.syntax		\
	sh.syntax		\
	smalltalk.syntax	\
	slang.syntax		\
	spec.syntax		\
	strace.syntax 		\
	sql.syntax		\
	swig.syntax		\
	syntax.syntax		\
	tcl.syntax		\
	texinfo.syntax		\
	vhdl.syntax		\
	unknown.syntax		\
	xml.syntax

SYNTAXCONFFILES =		\
	Syntax

if USE_EDIT
syntaxdir =		$(pkgdatadir)/syntax
syntax_DATA =		$(SYNTAXFILES)

syntaxconfdir =		$(sysconfdir)/@PACKAGE@
syntaxconf_DATA =	$(SYNTAXCONFFILES)
endif

EXTRA_DIST =		$(SYNTAXFILES) $(SYNTAXCONFFILES)
