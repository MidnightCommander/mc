#! /bin/sh
set -e

for i in "$@"; do
	i="./$i"
	sed '/^#:/d' < "$i" > "$i.tmp"
	mv -f "$i.tmp" "$i"
done
