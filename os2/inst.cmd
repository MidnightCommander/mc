@echo off
echo This file will setup your build environment for OS/2.
echo Press a key to continue...
pause
xcopy . ..\src
xcopy ..\nt\getopt.* ..\src
xcopy sys\. ..\src\sys\ /s
xcopy os2edit\. ..\edit /s
echo Now I will delete the files we don't need for OS/2.
echo Press a key to continue...
pause
cd ..\src
del achown.c
del chmod.c
del chown.c
del cons.handler.c
del cons.saver.c
del fixhlp.c
del key.c
del key.unx.c
del learn.c
del learn.h
del mad.c
del Makefile.in
del man2hlp.c
del mfmt.c
del slint.c
del utilunix.c
del xcurses.c
cd ..\edit
ren edit_key_translator.c edit_key_translator
echo Now you can use nmake to build the project.
