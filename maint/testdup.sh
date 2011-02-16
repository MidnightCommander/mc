#!/bin/sh

echo "test lib/*"
for i in `find src -name '*.[ch]'`; do
    ./maint/dupincludes.pl $i
done

echo "test srs/*"
for i in `find lib -name '*.[ch]'`; do
    ./maint/dupincludes.pl $i
done

