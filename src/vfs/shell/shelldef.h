
/**
 * \file
 * \brief Header: SHELL script defaults
 */

#ifndef MC__VFS_SHELL_DEF_H
#define MC__VFS_SHELL_DEF_H

/*** typedefs(not structures) and defined constants **********************************************/

/* default 'ls' script */
#define VFS_SHELL_LS_DEF_CONTENT ""                                       \
"export LC_TIME=C\n"                                                      \
"ls -Qlan \"/${SHELL_FILENAME}\" 2>/dev/null | grep '^[^cbt]' | (\n"      \
"while read p l u g s m d y n; do\n"                                      \
"    echo \"P$p $u.$g\"\n"                                                \
"    echo \"S$s\"\n"                                                      \
"    echo \"d$m $d $y\"\n"                                                \
"    echo \":$n\"\n"                                                      \
"    echo\n"                                                              \
"done\n"                                                                  \
")\n"                                                                     \
"ls -Qlan \"/${SHELL_FILENAME}\" 2>/dev/null | grep '^[cb]' | (\n"        \
"while read p l u g a i m d y n; do\n"                                    \
"    echo \"P$p $u.$g\"\n"                                                \
"    echo \"E$a$i\"\n"                                                    \
"    echo \"d$m $d $y\"\n"                                                \
"    echo \":$n\"\n"                                                      \
"    echo\n"                                                              \
"done\n"                                                                  \
")\n"                                                                     \
"echo \"### 200\"\n"

/* default file exists script */
#define VFS_SHELL_EXISTS_DEF_CONTENT ""                                   \
"ls -l \"/${SHELL_FILENAME}\" >/dev/null 2>/dev/null\n"                   \
"echo '### '$?\n"

/* default 'mkdir' script */
#define VFS_SHELL_MKDIR_DEF_CONTENT ""                                    \
"if mkdir \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                     \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'unlink' script */
#define VFS_SHELL_UNLINK_DEF_CONTENT ""                                   \
"if rm -f \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                     \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'chown' script */
#define VFS_SHELL_CHOWN_DEF_CONTENT ""                                          \
"if chown ${SHELL_FILEOWNER}:${SHELL_FILEGROUP} \"/${SHELL_FILENAME}\"; then\n" \
"    echo \"### 000\"\n"                                                        \
"else\n"                                                                        \
"    echo \"### 500\"\n"                                                        \
"fi\n"

/* default 'chmod' script */
#define VFS_SHELL_CHMOD_DEF_CONTENT ""                                      \
"if chmod ${SHELL_FILEMODE} \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"     \
"    echo \"### 000\"\n"                                                    \
"else\n"                                                                    \
"    echo \"### 500\"\n"                                                    \
"fi\n"

/* default 'utime' script */
#define VFS_SHELL_UTIME_DEF_CONTENT ""                                                                       \
"#UTIME \"$SHELL_TOUCHATIME_W_NSEC\" \"$SHELL_TOUCHMTIME_W_NSEC\" $SHELL_FILENAME\n"                         \
"if TZ=UTC touch -h -m -d \"$SHELL_TOUCHMTIME_W_NSEC\" \"/${SHELL_FILENAME}\" 2>/dev/null && \\\n"           \
"   TZ=UTC touch -h -a -d \"$SHELL_TOUCHATIME_W_NSEC\" \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"           \
"  echo \"### 000\"\n"                                                                                       \
"elif TZ=UTC touch -h -m -t $SHELL_TOUCHMTIME \"/${SHELL_FILENAME}\" 2>/dev/null && \\\n"                    \
"     TZ=UTC touch -h -a -t $SHELL_TOUCHATIME \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                    \
"  echo \"### 000\"\n"                                                                                       \
"elif [ -n \"$SHELL_HAVE_PERL\" ] && \\\n"                                                                   \
"   perl -e 'utime '$SHELL_FILEATIME','$SHELL_FILEMTIME',@ARGV;' \"/${SHELL_FILENAME}\" 2>/dev/null; then\n" \
"  echo \"### 000\"\n"                                                                                       \
"else\n"                                                                                                     \
"  echo \"### 500\"\n"                                                                                       \
"fi\n"

/* default 'rmdir' script */
#define VFS_SHELL_RMDIR_DEF_CONTENT ""                                      \
"if rmdir \"/${SHELL_FILENAME}\" 2>/dev/null; then\n"                       \
"   echo \"### 000\"\n"                                                     \
"else\n"                                                                    \
"   echo \"### 500\"\n"                                                     \
"fi\n"

/* default 'ln -s' symlink script */
#define VFS_SHELL_LN_DEF_CONTENT ""                                         \
"if ln -s \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"  \
"   echo \"### 000\"\n"                                                     \
"else\n"                                                                    \
"   echo \"### 500\"\n"                                                     \
"fi\n"

/* default 'mv' script */
#define VFS_SHELL_MV_DEF_CONTENT ""                                         \
"if mv \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"     \
"   echo \"### 000\"\n"                                                     \
"else\n"                                                                    \
"   echo \"### 500\"\n"                                                     \
"fi\n"

/* default 'ln' hardlink script */
#define VFS_SHELL_HARDLINK_DEF_CONTENT ""                                   \
"if ln \"/${SHELL_FILEFROM}\" \"/${SHELL_FILETO}\" 2>/dev/null; then\n"     \
"   echo \"### 000\"\n"                                                     \
"else\n"                                                                    \
"   echo \"### 500\"\n"                                                     \
"fi\n"

/* default 'retr'  script */
#define VFS_SHELL_GET_DEF_CONTENT ""                                             \
"export LC_TIME=C\n"                                                             \
"if dd if=\"/${SHELL_FILENAME}\" of=/dev/null bs=1 count=1 2>/dev/null ; then\n" \
"    ls -ln \"/${SHELL_FILENAME}\" 2>/dev/null | (\n"                            \
"       read p l u g s r\n"                                                      \
"       echo $s\n"                                                               \
"    )\n"                                                                        \
"    echo \"### 100\"\n"                                                         \
"    cat \"/${SHELL_FILENAME}\"\n"                                               \
"    echo \"### 200\"\n"                                                         \
"else\n"                                                                         \
"    echo \"### 500\"\n"                                                         \
"fi\n"

/* default 'stor'  script */
#define VFS_SHELL_SEND_DEF_CONTENT ""                                     \
"FILENAME=\"/${SHELL_FILENAME}\"\n"                                       \
"FILESIZE=${SHELL_FILESIZE}\n"                                            \
"echo \"### 001\"\n"                                                      \
"{\n"                                                                     \
"    while [ $FILESIZE -gt 0 ]; do\n"                                     \
"        cnt=`expr \\( $FILESIZE + 255 \\) / 256`\n"                      \
"        n=`dd bs=256 count=$cnt | tee -a \"${FILENAME}\" | wc -c`\n"     \
"        FILESIZE=`expr $FILESIZE - $n`\n"                                \
"    done\n"                                                              \
"}; echo \"### 200\"\n"

/* default 'appe'  script */
#define VFS_SHELL_APPEND_DEF_CONTENT ""                                   \
"FILENAME=\"/${SHELL_FILENAME}\"\n"                                       \
"FILESIZE=${SHELL_FILESIZE}\n"                                            \
"echo \"### 001\"\n"                                                      \
"res=`exec 3>&1\n"                                                        \
"(\n"                                                                     \
"    head -c $FILESIZE -q - || echo DD >&3\n"                             \
") 2>/dev/null | (\n"                                                     \
"    cat > \"${FILENAME}\"\n"                                             \
"    cat > /dev/null\n"                                                   \
")`; [ \"$res\" = DD ] && {\n"                                            \
"    > \"${FILENAME}\"\n"                                                 \
"    while [ $FILESIZE -gt 0 ]\n"                                         \
"    do\n"                                                                \
"       cnt=`expr \\( $FILESIZE + 255 \\) / 256`\n"                       \
"       n=`dd bs=256 count=$cnt | tee -a \"${FILENAME}\" | wc -c`\n"      \
"       FILESIZE=`expr $FILESIZE - $n`\n"                                 \
"    done\n"                                                              \
"}; echo \"### 200\"\n"

/* default 'info'  script */
#define VFS_SHELL_INFO_DEF_CONTENT ""                                     \
"export LC_TIME=C\n"                                                      \
"#SHELL_HAVE_HEAD         1\n"                                            \
"#SHELL_HAVE_SED          2\n"                                            \
"#SHELL_HAVE_AWK          4\n"                                            \
"#SHELL_HAVE_PERL         8\n"                                            \
"#SHELL_HAVE_LSQ         16\n"                                            \
"#SHELL_HAVE_DATE_MDYT   32\n"                                            \
"#SHELL_HAVE_TAIL        64\n"                                            \
"res=0\n"                                                                 \
"if `echo yes| head -c 1 > /dev/null 2>&1` ; then\n"                      \
"    res=`expr $res + 1`\n"                                               \
"fi\n"                                                                    \
"if `sed --version >/dev/null 2>&1` ; then\n"                             \
"    res=`expr $res + 2`\n"                                               \
"fi\n"                                                                    \
"if `awk --version > /dev/null 2>&1` ; then\n"                            \
"    res=`expr $res + 4`\n"                                               \
"fi\n"                                                                    \
"if `perl -v > /dev/null 2>&1` ; then\n"                                  \
"    res=`expr $res + 8`\n"                                               \
"fi\n"                                                                    \
"if `ls -Q / >/dev/null 2>&1` ; then\n"                                   \
"    res=`expr $res + 16`\n"                                              \
"fi\n"                                                                    \
"dat=`ls -lan / 2>/dev/null | head -n 3 | tail -n 1 | (\n"                \
"    while read p l u g s rec; do\n"                                      \
"        if [ -n \"$g\" ]; then\n"                                        \
"            if [ -n \"$l\" ]; then\n"                                    \
"                echo \"$rec\"\n"                                         \
"            fi\n"                                                        \
"        fi\n"                                                            \
"    done\n"                                                              \
") | cut -c1 2>/dev/null`\n"                                              \
"r=`echo \"0123456789\"| grep \"$dat\"`\n"                                \
"if [ -z \"$r\" ]; then\n"                                                \
"    res=`expr $res + 32`\n"                                              \
"fi\n"                                                                    \
"if `echo yes| tail -c +1 - > /dev/null 2>&1` ; then\n"                   \
"    res=`expr $res + 64`\n"                                              \
"fi\n"                                                                    \
"echo $res\n"                                                             \
"echo \"### 200\"\n"

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

/*** inline functions ****************************************************************************/

#endif /* MC__VFS_SHELL_DEF_H */
