#!/bin/sh

xgettext --default-domain=mc --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f mc.po \
   || ( rm -f ./mc.pot \
    && mv mc.po ./mc.pot )
