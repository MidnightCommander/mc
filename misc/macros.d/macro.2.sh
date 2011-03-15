#silent
l       Lower case selection
        TMPFILE=`mktemp ${MC_TMPDIR:-/tmp}/up.XXXXXX` || exit 1
        cat %b > $TMPFILE
        cat $TMPFILE| sed 's/\(.*\)/\L\1/' >%b
        rm -f $TMPFILE
