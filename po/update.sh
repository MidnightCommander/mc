#!/bin/sh

PACKAGE="mc"

if [ "x$1" = "x--help" ]; then

echo Usage: ./update.sh langcode
echo --help                  display this help and exit
echo --missing	             search for missing files in POTFILES.in
echo
echo Examples of use:
echo ./update.sh ----- just creates a new pot file from the source
echo ./update.sh da -- created new pot file and updated the da.po file 

elif [ "x$1" = "x--missing" ]; then

echo "Searching for files containing _( ) but missing in POTFILES.in..."
find ../ -regex '.*\.[c|y|cc|c++|h]' | xargs grep _\( | cut -d: -f1 | uniq | cut -d/ -f2- > POTFILES.in.missing

echo Sorting... comparing...
sort -d POTFILES.in -o POTFILES.in
sort -d POTFILES.in.missing -o POTFILES.in.missing

diff POTFILES.in POTFILES.in.missing -u0 | grep '^+' |grep -v '^+++'|grep -v '^@@' > POTFILES.in.missing

if [ -s POTFILES.in.missing ]; then
echo && echo "Here are the results:"
echo && cat POTFILES.in.missing
echo && echo "File POTFILES.in.missing is being placed in directory..."

else

echo &&echo "There are no missing files, thanks God!"
rm POTFILES.in.missing

fi 

elif [ "x$1" = "x" ]; then 

echo "Building the $PACKAGE.pot ..."

xgettext --default-domain=$PACKAGE --directory=.. \
  --add-comments=TRANSLATORS: --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $PACKAGE.po \
   || ( rm -f ./$PACKAGE.pot \
&& mv $PACKAGE.po ./$PACKAGE.pot );

else

if [ -s $1.po ]; then

xgettext --default-domain=$PACKAGE --directory=.. \
  --add-comments=TRANSLATORS: --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f $PACKAGE.po \
   || ( rm -f ./PACKAGE.pot \
&& mv $PACKAGE.po ./$PACKAGE.pot );

echo "Building the $PACKAGE.pot ..."
echo "Now merging $1.po with $PACKAGE.pot, and creating an updated $1.po ..." 

mv $1.po $1.po.old && msgmerge --no-location $1.po.old $PACKAGE.pot -o $1.po \
&& rm $1.po.old;

msgfmt --statistics $1.po

else

echo Sorry $1.po does not exist!

fi;

fi;
