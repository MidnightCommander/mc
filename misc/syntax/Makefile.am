SYNTAXFILES =			\
	PKGBUILD.syntax		\
	ada95.syntax		\
	aspx.syntax		\
	assembler.syntax	\
	awk.syntax		\
	c.syntax		\
	cabal.syntax		\
	changelog.syntax	\
	cobol.syntax		\
	cs.syntax		\
	css.syntax		\
	cxx.syntax		\
	d.syntax		\
	debian-changelog.syntax	\
	debian-control.syntax	\
	debian-description.syntax	\
	debian-sources-list.syntax	\
	diff.syntax		\
	dos.syntax		\
	ebuild.syntax		\
	eiffel.syntax		\
	erlang.syntax		\
	f90.syntax		\
	filehighlight.syntax	\
	fortran.syntax		\
	haskell.syntax		\
	html.syntax		\
	idl.syntax		\
	j.syntax		\
	java.syntax		\
	js.syntax		\
	latex.syntax		\
	lisp.syntax		\
	lsm.syntax		\
	lua.syntax		\
	m4.syntax		\
	mail.syntax		\
	makefile.syntax		\
	ml.syntax		\
	named.syntax		\
	nemerle.syntax		\
	nroff.syntax		\
	octave.syntax		\
	pascal.syntax		\
	perl.syntax		\
	php.syntax		\
	po.syntax		\
	povray.syntax		\
	procmail.syntax		\
	properties.syntax	\
	python.syntax		\
	ruby.syntax		\
	sh.syntax		\
	slang.syntax		\
	smalltalk.syntax	\
	spec.syntax		\
	sql.syntax		\
	strace.syntax 		\
	swig.syntax		\
	syntax.syntax		\
	tcl.syntax		\
	texinfo.syntax		\
	tt.syntax		\
	unknown.syntax		\
	verilog.syntax		\
	vhdl.syntax		\
	xml.syntax		\
	yum-repo.syntax		\
	yxx.syntax

SYNTAXCONFFILES =		\
	Syntax

if USE_EDIT
syntaxdir =		$(pkgdatadir)/syntax
syntax_DATA =		$(SYNTAXFILES)

syntaxconfdir =		$(sysconfdir)/@PACKAGE@
syntaxconf_DATA =	$(SYNTAXCONFFILES)
endif

EXTRA_DIST =		$(SYNTAXFILES) $(SYNTAXCONFFILES)
