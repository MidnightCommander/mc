mc ()
{
	MC=/tmp/mc$$-"$RANDOM"
	/usr/bin/mc -P "$@" > "$MC"
	cd "`cat $MC`"
	rm "$MC"
        unset MC;
}
