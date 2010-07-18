
/**
 * \file
 * \brief Header: FISH script defaults
 */

#ifndef MC_FISH_DEF_H
#define MC_FISH_DEF_H

/* default 'ls' script */
#define FISH_LS_DEF_CONTENT ""                                            \
"#LIST /${FISH_DIR}\n"                                                    \
"export LC_TIME=C\n"                                                      \
"FISH_DIR=\"/${FISH_PARAM1}\"\n"                                          \
"ls -lan \"${FISH_DIR}\" 2>/dev/null | grep '^[^cbt]' | (\n"              \
"while read p l u g s m d y n n2 n3; do\n"                                \
"    if [ -n \"$g\" ]; then\n"                                            \
"        if [ \"$m\" = \"0\" ]; then\n"                                   \
"            s=$d; m=$y; d=$n; y=$n2; n=$n3\n"                            \
"        else\n"                                                          \
"            n=$n\" \"$n2\" \"$n3\n"                                      \
"        fi\n"                                                            \
"        echo \"P$p $u $g\"\n"                                            \
"        echo \"S$s\"\n"                                                  \
"        echo \"d$m $d $y\"\n"                                            \
"        echo \":\"$n\n"                                                  \
"        echo\n"                                                          \
"    fi\n"                                                                \
"done\n"                                                                  \
")\n"                                                                     \
"ls -lan \"${FISH_DIR}\" 2>/dev/null | grep '^[cb]' | (\n"                \
"while read p l u g a i m d y n n2 n3; do\n"                              \
"    if [ -n \"$g\" ]; then\n"                                            \
"        if [ \"$a\" = \"0\" ]; then\n"                                   \
"            a=$m; i=$d; m=$y; d=$n; y=$n2; n=$n3\n"                      \
"        else\n"                                                          \
"            n=$n\" \"$n2\" \"$n3\n"                                      \
"        fi\n"                                                            \
"        echo \"P$p $u $g\"\n"                                            \
"        echo \"S$s\"\n"                                                  \
"        echo \"d$m $d $y\"\n"                                            \
"        echo \":\"$n\n"                                                  \
"        echo\n"                                                          \
"    fi\n"                                                                \
"done\n"                                                                  \
")\n"                                                                     \
"echo \"### 200\"\n"

/* default file exisits script */
#define FISH_EXISTS_DEF_CONTENT ""                                        \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"#ISEXISTS $FILENAME\n"                                                   \
"ls -l \"${FILENAME}\" >/dev/null 2>/dev/null\n"                          \
"echo '### '$?\n"

/* default 'mkdir' script */
#define FISH_MKDIR_DEF_CONTENT ""                                         \
"FILENAME=/${FISH_PARAM1}\n"                                              \
"#MKD $FILENAME\n"                                                        \
"if mkdir \"$FILENAME\" 2>/dev/null; then\n"                              \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'unlink' script */
#define FISH_UNLINK_DEF_CONTENT ""                                        \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"#DELE $FILENAME\n"                                                       \
"if rm -f \"${FILENAME}\" 2>/dev/null; then\n"                            \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'chown' script */
#define FISH_CHOWN_DEF_CONTENT ""                                         \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"NEWUSER=\"${FISH_PARAM2}\"\n"                                            \
"NEWGROUP=\"${FISH_PARAM3}\"\n"                                           \
"#CHOWN $NEWUSER:$NEWGROUP $FILENAME\n"                                   \
"if chown ${NEWUSER}:${NEWGROUP} \"${FILENAME}\" ; then\n"                \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'chmod' script */
#define FISH_CHMOD_DEF_CONTENT ""                                         \
"FISH_FILENAME=\"/${FISH_PARAM1}\"\n"                                     \
"FISH_MODE=\"${FISH_PARAM2}\"\n"                                          \
"#CHMOD $FISH_MODE $FISH_FILENAME\n"                                      \
"if chmod ${FISH_MODE} \"${FISH_FILENAME} 2>/dev/null; then\n"            \
"    echo \"### 000\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'rmdir' script */
#define FISH_RMDIR_DEF_CONTENT ""                                         \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"#RMD $FILENAME\n"                                                        \
"if rmdir \"${FILENAME}\" 2>/dev/null; then\n"                            \
"   echo \"### 000\"\n"                                                   \
"else\n"                                                                  \
"   echo \"### 500\"\n"                                                   \
"fi\n"

/* default 'ln -s' symlink script */
#define FISH_LN_DEF_CONTENT ""                                            \
"FILEFROM=\"${FISH_PARAM1}\"\n"                                           \
"FILETO=\"/${FISH_PARAM2}\"\n"                                            \
"#SYMLINK $FILEFROM $FILETO\n"                                            \
"if ln -s \"${FILEFROM}\" \"${FILETO}\" 2>/dev/null; then\n"              \
"   echo \"### 000\"\n"                                                   \
"else\n"                                                                  \
"   echo \"### 500\"\n"                                                   \
"fi\n"

/* default 'mv' script */
#define FISH_MV_DEF_CONTENT ""                                            \
"FILEFROM=\"/${FISH_PARAM1}\"\n"                                          \
"FILETO=\"/${FISH_PARAM2}\"\n"                                            \
"#RENAME $FILEFROM $FILETO\n"                                             \
"if mv \"${FILEFROM}\" \"${FILETO}\" 2>/dev/null; then\n"                 \
"   echo \"### 000\"\n"                                                   \
"else\n"                                                                  \
"   echo \"### 500\"\n"                                                   \
"fi\n"

/* default 'ln' hardlink script */
#define FISH_HARDLINK_DEF_CONTENT ""                                      \
"FILEFROM=\"/${FISH_PARAM1}\"\n"                                          \
"FILETO=\"/${FISH_PARAM2}\"\n"                                            \
"#LINK $FILEFROM $FILETO\n"                                               \
"if ln \"${FILEFROM}\" \"${FILETO}\" 2>/dev/null; then\n"                 \
"   echo \"### 000\"\n"                                                   \
"else\n"                                                                  \
"   echo \"### 500\"\n"                                                   \
"fi\n"

/* default 'retr'  script */
#define FISH_GET_DEF_CONTENT ""                                           \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"export LC_TIME=C\n"                                                      \
"#RETR $FILENAME\n"                                                       \
"if dd if=\"${FILENAME}\" of=/dev/null bs=1 count=1 2>/dev/null ; then\n" \
"    ls -ln \"${FILENAME}\" 2>/dev/null | (\n"                            \
"       read p l u g s r\n"                                               \
"       echo $s\n"                                                        \
"    )\n"                                                                 \
"    echo \"### 100\"\n"                                                  \
"    cat \"${FILENAME}\"\n"                                               \
"    echo \"### 200\"\n"                                                  \
"else\n"                                                                  \
"    echo \"### 500\"\n"                                                  \
"fi\n"

/* default 'stor'  script */
#define FISH_SEND_DEF_CONTENT ""                                          \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"FILESIZE=${FISH_PARAM2}\n"                                               \
"#STOR $FILESIZE $FILENAME\n"                                             \
"echo \"### 001\"\n"                                                      \
"{\n"                                                                     \
"    while [ $FILESIZE -gt 0 ]; do\n"                                     \
"        cnt=`expr \\( $FILESIZE + 255 \\) / 256`\n"                      \
"        n=`dd bs=256 count=$cnt | tee -a \"${FILENAME}\" | wc -c`\n"     \
"        FILESIZE=`expr $FILESIZE - $n`\n"                                \
"    done\n"                                                              \
"}; echo \"### 200\"\n"

/* default 'appe'  script */
#define FISH_APPEND_DEF_CONTENT ""                                        \
"FILENAME=\"/${FISH_PARAM1}\"\n"                                          \
"FILESIZE=${FISH_PARAM2}\n"                                               \
"#APPE $FILESIZE $FILENAME\n"                                             \
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
#define FISH_INFO_DEF_CONTENT ""                                          \
"export LC_TIME=C\n"                                                      \
"#FISH_HAVE_HEAD         1\n"                                             \
"#FISH_HAVE_SED          2\n"                                             \
"#FISH_HAVE_AWK          4\n"                                             \
"#FISH_HAVE_PERL         8\n"                                             \
"#FISH_HAVE_LSQ         16\n"                                             \
"#FISH_HAVE_DATE_MDYT   32\n"                                             \
"#FISH_HAVE_TAIL        64\n"                                             \
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


#endif
