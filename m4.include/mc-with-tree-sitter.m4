dnl
dnl Tree-sitter syntax highlighting support.
dnl

AC_DEFUN([mc_WITH_TREE_SITTER], [

    AC_ARG_WITH([tree-sitter],
        AS_HELP_STRING([--with-tree-sitter],
            [Enable tree-sitter syntax highlighting (in addition to legacy MC highlighting)]),
        [with_tree_sitter=$withval],
        [with_tree_sitter=no])

    AC_ARG_WITH([tree-sitter-grammars],
        AS_HELP_STRING([--with-tree-sitter-grammars=LIST],
            [Comma-separated list of tree-sitter grammars to build (default: all)]),
        [with_tree_sitter_grammars=$withval],
        [with_tree_sitter_grammars=all])

    AC_ARG_WITH([tree-sitter-static],
        AS_HELP_STRING([--with-tree-sitter-static],
            [Link tree-sitter grammars statically into the binary (default: build as shared modules)]),
        [with_tree_sitter_static=$withval],
        [with_tree_sitter_static=no])

    if test x"$with_tree_sitter" = xyes; then
        AC_CHECK_HEADER([tree_sitter/api.h], [],
            [AC_MSG_ERROR([tree-sitter headers not found (required for --with-tree-sitter)])])
        AC_CHECK_LIB([tree-sitter], [ts_parser_new], [],
            [AC_MSG_ERROR([tree-sitter library not found (required for --with-tree-sitter)])])
        AC_DEFINE([HAVE_TREE_SITTER], [1], [Define if tree-sitter syntax highlighting is enabled])

        if test x"$with_tree_sitter_static" = xyes; then
            AC_DEFINE([TREE_SITTER_STATIC], [1], [Define if tree-sitter grammars are linked statically])
            TREE_SITTER_BUILD_TARGET="libtsgrammars.la"
            TREE_SITTER_BUILD_MODE="static"
        else
            AC_DEFINE([TREE_SITTER_SHARED], [1], [Define if tree-sitter grammars are loaded as shared modules])
            PKG_CHECK_MODULES([GMODULE], [gmodule-2.0], [],
                [AC_MSG_ERROR([gmodule-2.0 required for shared tree-sitter grammars (use --with-tree-sitter-static to avoid)])])
            TREE_SITTER_BUILD_TARGET="shared-modules"
            TREE_SITTER_BUILD_MODE="shared"
            dnl Resolve libdir for use in Makefile (which doesn't have automake's variable chain)
            eval "ts_libdir=\"$libdir\""
            TREE_SITTER_GRAMMAR_LIBDIR="${ts_libdir}/mc/ts-grammars"
        fi
        AC_SUBST([TREE_SITTER_BUILD_TARGET])
        AC_SUBST([TREE_SITTER_BUILD_MODE])
        AC_SUBST([TREE_SITTER_GRAMMAR_LIBDIR])

        dnl Known grammar directory names (must match download-grammars.sh)
        all_ts_grammars="ada asm awk bash bison c caddy cmake cobol cpp c_sharp css cuda d diff dockerfile dot erlang fortran glsl go haskell hcl html idl ini java javascript json kotlin lisp lua make markdown markdown_inline matlab meson muttrc ocaml pascal perl php po properties proto python qmljs r ruby rust scala smalltalk sql strace swift tcl toml turtle typescript verilog vhdl xml yaml"

        dnl Grammars that have scanner.c
        ts_scanner_c="awk bash bison caddy cmake cobol cpp c_sharp css cuda d dockerfile fortran haskell hcl html javascript kotlin lua markdown markdown_inline matlab ocaml perl php properties python qmljs r ruby rust scala tcl toml typescript xml yaml"

        dnl Grammars that have scanner.cc (C++ scanner)
        ts_scanner_cc="sql"

        dnl Parse and validate selected grammars
        if test x"$with_tree_sitter_grammars" = xall; then
            tree_sitter_grammars="$all_ts_grammars"
        else
            tree_sitter_grammars=`echo "$with_tree_sitter_grammars" | tr ',' ' '`
            for g in $tree_sitter_grammars; do
                case " $all_ts_grammars " in
                    *" $g "*) ;;
                    *) AC_MSG_ERROR([unknown tree-sitter grammar: $g
Valid grammars: $all_ts_grammars]) ;;
                esac
            done
        fi

        dnl Build source/object file lists, -D flags, shared lib list, and grammar dir list
        TREE_SITTER_GRAMMAR_SOURCES=""
        TREE_SITTER_GRAMMAR_OBJECTS=""
        TREE_SITTER_GRAMMAR_DEFS=""
        TREE_SITTER_SHARED_LIBS=""
        TREE_SITTER_GRAMMARS=""
        ts_need_cxx=no
        for g in $tree_sitter_grammars; do
            TREE_SITTER_GRAMMAR_SOURCES="$TREE_SITTER_GRAMMAR_SOURCES $g/parser.c"
            TREE_SITTER_GRAMMAR_OBJECTS="$TREE_SITTER_GRAMMAR_OBJECTS $g/parser.lo"
            case " $ts_scanner_c " in
                *" $g "*)
                    TREE_SITTER_GRAMMAR_SOURCES="$TREE_SITTER_GRAMMAR_SOURCES $g/scanner.c"
                    TREE_SITTER_GRAMMAR_OBJECTS="$TREE_SITTER_GRAMMAR_OBJECTS $g/scanner.lo" ;;
            esac
            case " $ts_scanner_cc " in
                *" $g "*)
                    TREE_SITTER_GRAMMAR_SOURCES="$TREE_SITTER_GRAMMAR_SOURCES $g/scanner.cc"
                    TREE_SITTER_GRAMMAR_OBJECTS="$TREE_SITTER_GRAMMAR_OBJECTS $g/scanner.lo"
                    ts_need_cxx=yes ;;
            esac
            if test x"$with_tree_sitter_static" = xyes; then
                upper=`echo "$g" | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
                TREE_SITTER_GRAMMAR_DEFS="$TREE_SITTER_GRAMMAR_DEFS -DHAVE_GRAMMAR_${upper}=1"
            fi
            TREE_SITTER_SHARED_LIBS="$TREE_SITTER_SHARED_LIBS mc-ts-$g.la"
            TREE_SITTER_GRAMMARS="$TREE_SITTER_GRAMMARS $g"
        done
        dnl Remove leading spaces
        TREE_SITTER_GRAMMAR_SOURCES=`echo $TREE_SITTER_GRAMMAR_SOURCES`
        TREE_SITTER_GRAMMAR_OBJECTS=`echo $TREE_SITTER_GRAMMAR_OBJECTS`
        TREE_SITTER_GRAMMAR_DEFS=`echo $TREE_SITTER_GRAMMAR_DEFS`
        TREE_SITTER_SHARED_LIBS=`echo $TREE_SITTER_SHARED_LIBS`
        TREE_SITTER_GRAMMARS=`echo $TREE_SITTER_GRAMMARS`

        AC_SUBST([TREE_SITTER_GRAMMAR_SOURCES])
        AC_SUBST([TREE_SITTER_GRAMMAR_OBJECTS])
        AC_SUBST([TREE_SITTER_GRAMMAR_DEFS])
        AC_SUBST([TREE_SITTER_SHARED_LIBS])
        AC_SUBST([TREE_SITTER_GRAMMARS])

        if test x"$ts_need_cxx" = xyes; then
            TREE_SITTER_LIBS="-ltree-sitter -lstdc++"
        else
            TREE_SITTER_LIBS="-ltree-sitter"
        fi
        TREE_SITTER_CFLAGS=""
        AC_SUBST([TREE_SITTER_LIBS])
        AC_SUBST([TREE_SITTER_CFLAGS])

        ts_grammar_count=`echo $tree_sitter_grammars | wc -w | tr -d ' '`
        if test x"$with_tree_sitter_grammars" = xall; then
            AC_MSG_NOTICE([tree-sitter: enabled ($TREE_SITTER_BUILD_MODE) with all $ts_grammar_count grammars])
        else
            AC_MSG_NOTICE([tree-sitter: enabled ($TREE_SITTER_BUILD_MODE) with $ts_grammar_count grammars: $TREE_SITTER_GRAMMARS])
        fi

        dnl Auto-download missing grammar sources
        ts_grammars_missing=""
        for g in $tree_sitter_grammars; do
            if test ! -f "$srcdir/src/editor/ts-grammars/$g/parser.c"; then
                ts_grammars_missing="$ts_grammars_missing $g"
            fi
        done

        if test -n "$ts_grammars_missing"; then
            ts_grammars_dir="$srcdir/src/editor/ts-grammars"
            if test -n "$TREE_SITTER_GRAMMARS_DIR"; then
                AC_MSG_NOTICE([copying missing grammar sources from $TREE_SITTER_GRAMMARS_DIR...])
                for g in $ts_grammars_missing; do
                    if test -d "$TREE_SITTER_GRAMMARS_DIR/$g"; then
                        mkdir -p "$ts_grammars_dir/$g"
                        cp -a "$TREE_SITTER_GRAMMARS_DIR/$g"/* "$ts_grammars_dir/$g/"
                    else
                        AC_MSG_ERROR([grammar $g not found in $TREE_SITTER_GRAMMARS_DIR])
                    fi
                done
            else
                AC_MSG_NOTICE([downloading missing grammar sources...])
                (cd "$ts_grammars_dir" && $SHELL download-grammars.sh $ts_grammars_missing) || \
                    AC_MSG_ERROR([failed to download grammar sources.
For offline builds, set TREE_SITTER_GRAMMARS_DIR=/path/to/grammars])
            fi
        fi
    else
        AC_MSG_NOTICE([tree-sitter syntax highlighting disabled])
    fi
])
