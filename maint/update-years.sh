#! /bin/sh

YEAR=`date +%Y`

SOURCES="`find lib src tests -name '*.c'`"
SOURCES="$SOURCES src/man2hlp/man2hlp.in"
SOURCES="$SOURCES tests/src/vfs/extfs/helpers-list/test_all"

LINE="Copyright (C)"

for i in $SOURCES; do
    # replace year: XXXX-YYYY -> XXXX-ZZZZ
    # add year: XXXX -> XXXX-ZZZZ
    ${SED-sed} -e "
        1,20 {
                /$LINE/s/-[0-9]\{4\}$/-$YEAR/
        };
        1,20 {
                /$LINE/s/ [0-9]\{4\}$/&-$YEAR/
    }" $i > $i.tmp && mv -f $i.tmp $i
done

# special case
${SED-sed} -e "/$LINE/s/-[0-9]\{4\} the/-$YEAR the/" lib/global.c > lib/global.c.tmp && \
  mv -f lib/global.c.tmp lib/global.c

# restore permissions
chmod 755 tests/src/vfs/extfs/helpers-list/test_all
