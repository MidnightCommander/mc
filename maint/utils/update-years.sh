#! /bin/sh

YEAR=`date +%Y`

SOURCES="`find lib src tests -name '*.c'`"
SOURCES="$SOURCES src/man2hlp/man2hlp.in"

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
${SED-sed} -e "/$LINE/s/-[0-9]\{4\} the/-$YEAR the/" src/editor/editwidget.c > src/editor/editwidget.c.tmp && \
  mv -f src/editor/editwidget.c.tmp src/editor/editwidget.c
