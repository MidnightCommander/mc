#silent
S       Sort selection
        TMPFILE=`mktemp ${MC_TMPDIR:-/tmp}/up.XXXXXX` || exit 1
        cat %b > $TMPFILE
        cat $TMPFILE| sort >%b
        rm -f $TMPFILE
