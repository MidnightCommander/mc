/*find . -type f -name "*.[ch]" | etags -l c --declarations - */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>
#include "etags.h"

long get_pos_from(char *str)
{
    static char buf[16];
    int i, j;
    j = 0;
    int len = strlen( str );
    for ( i = 0; i < len && i < 16; i++ ) {
        char c = (char) str[i];
        if ( isdigit (c) ) {
            buf[j++] = c;
            buf[j] = '\0';
        } else {
            return atol((char *)buf);
        }
    }
    return 0;
}

int set_def_hash(char *tagfile, char *start_path, char *match_func, struct def_hash_type *def_hash, int *num)
{
    FILE *f;
    static char buf[4048];
    int len;

    f = fopen (tagfile, "r");
    int i;
    if (!f)
        return 1;
    len = strlen( match_func );
    int pos;

    char *fullpath = NULL;
    char *filename = NULL;
    long line;
    enum {start, in_filename, in_define} state = start;

    while (fgets (buf, sizeof (buf), f)) {

        switch ( state ) {
        case start:
            if ( buf[0] == 0x0C ) {
                state = in_filename;
            }
            break;
        case in_filename:
            pos = strcspn(buf, ",");
            g_free(filename);
            filename = malloc(pos + 2);
            g_strlcpy ( filename, (char *)buf, pos + 1 );
            state = in_define;
            break;
        case in_define:
            if ( buf[0] == 0x0C ) {
                state = in_filename;
                break;
            }
            /* check if the filename matches the define pos */
            if ( strstr ( buf, match_func ) ) {
                int l = (int)strlen( buf );
                for ( i = 0; i < l; i++) {
                    if ( ( buf[i] == 0x7F || buf[i] == 0x01 ) && isdigit(buf[i+1]) ) {
                        line = get_pos_from(&buf[i+1]);
                        state = start;
                        if ( *num < MAX_DEFINITIONS ) {
                            def_hash[*num].filename_len = strlen(filename);
                            fullpath = g_strdup_printf("%s/%s",start_path, filename);
                            def_hash[*num].filename = g_strdup(fullpath);
                            g_free(fullpath);
                            def_hash[*num].line = line;
                            (*num)++;
                        }
                    }
                }
            }
            break;
        }
    }
    g_free(filename);
    return 0;
}
