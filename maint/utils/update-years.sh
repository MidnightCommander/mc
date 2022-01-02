#! /bin/sh

YEAR=`date +%Y`

SOURCES="`find lib src tests -name '*.c'`"

LINE="Copyright (C)"

for i in "$SOURCES"; do
    # replace year: XXXX-YYYY -> XXXX-ZZZZ
    # add year: XXXX -> XXXX-ZZZZ
    sed -i -e "
        1,20 {
                /$LINE/s/-[0-9]\{4\}$/-$YEAR/
        };
        1,20 {
                /$LINE/s/ [0-9]\{4\}$/&-$YEAR/
    }" $i
done

# special case
sed -i -e "/$LINE/s/-[0-9]\{4\} the/-$YEAR the/" src/editor/editwidget.c
