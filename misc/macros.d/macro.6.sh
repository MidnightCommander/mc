#silent
e       execute something
        TMPFILE=`mktemp ${MC_TMPDIR:-/tmp}/up.XXXXXX` || exit 1
        cat %b > $TMPFILE
        sh $TMPFILE > %b
