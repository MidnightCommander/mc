#include <config.h>

#ifdef HAVE_CHARSET
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>

#include "charsets.h"
#include "i18n.h"

int n_codepages = 0;

struct codepage_desc *codepages;

uchar conv_displ[256];
uchar conv_input[256];
uchar printable[256];

int load_codepages_list()
{
    int result = -1;
    FILE *f;
    char buf[256];
    extern char* mc_home;
    extern int display_codepage;
    char * default_codepage = NULL;

    strcpy ( buf, mc_home );
    strcat ( buf, "/mc.charsets" );
    if ( !( f = fopen( buf, "r" ) ) )
	return -1;

    for ( n_codepages=0; fgets( buf, sizeof buf, f ); )
	if ( buf[0] != '\n' && buf[0] != '\0' && buf [0] != '#' )
	    ++n_codepages;
    rewind( f );

    codepages = calloc( n_codepages + 1, sizeof(struct codepage_desc) );

    for( n_codepages = 0; fgets( buf, sizeof buf, f ); ) {
	/* split string into id and cpname */
	char *p = buf;
	int buflen = strlen( buf );

	if ( *p == '\n' || *p == '\0' || *p == '#')
	    continue;

	if ( buflen > 0 && buf[ buflen - 1 ] == '\n' )
	    buf[ buflen - 1 ] = '\0';
	while ( *p != '\t' && *p != ' ' && *p != '\0' )
	    ++p;
	if ( *p == '\0' )
	    goto fail;

	*p++ = 0;

	while ( *p == '\t' || *p == ' ' )
	    ++p;
	if ( *p == '\0' )
	    goto fail;

	if (strcmp (buf, "default") == 0) {
	    default_codepage = strdup (p);
	    continue;
	}

	codepages[n_codepages].id = strdup( buf  );
	codepages[n_codepages].name = strdup( p );
	++n_codepages;
    }

    if (default_codepage) {
	display_codepage = get_codepage_index (default_codepage);
	free (default_codepage);
    }

    result = n_codepages;
fail:
    fclose( f );
    return result;
}

#define OTHER_8BIT "Other_8_bit"

char *get_codepage_id( int n )
{
    return (n < 0) ? OTHER_8BIT : codepages[ n ].id;
}

int get_codepage_index( const char *id )
{
    int i;
    if (strcmp( id, OTHER_8BIT ) == 0)
	return -1;
    for ( i=0; codepages[ i ].id; ++i )
	if (strcmp( id, codepages[ i ].id ) == 0)
	    return i;
    return -1;
}

static char translate_character( iconv_t cd, const char c )
{
    char outbuf[4], *obuf;
    size_t ibuflen, obuflen, count;

    const char *ibuf = &c; 
    obuf = outbuf;
    ibuflen = 1; obuflen = 4;

    count = iconv(cd, &ibuf, &ibuflen, &obuf, &obuflen);
    if (count >= 0 && ibuflen == 0)
	return outbuf[0];

    return UNKNCHAR;
}

char errbuf[255];

char* init_translation_table( int cpsource, int cpdisplay )
{
    int i;
    iconv_t cd;
    uchar ch;
    char *cpsour, *cpdisp;

    /* Fill inpit <-> display tables */

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay) {
	for (i=0; i<=255; ++i) {
	    conv_displ[i] = i;
	    conv_input[i] = i;
	    printable[i] = (i > 31 && i != 127);
	}
	return NULL;
    }

    for (i=0; i<=127; ++i) {
	conv_displ[i] = i;
	conv_input[i] = i;
	printable[i] = (i > 31 && i != 127);
    }

    cpsour = codepages[ cpsource ].id;
    cpdisp = codepages[ cpdisplay ].id;

    /* display <- inpit table */

    cd = iconv_open( cpdisp, cpsour );
    if (cd == (iconv_t) -1) {
	sprintf( errbuf, _("Can't translate from %s to %s"), cpsour, cpdisp );
	return errbuf;
    }

    for (i=128; i<=255; ++i)
	conv_displ[i] = translate_character( cd, i );

    iconv_close( cd );

    /* inpit <- display table */

    cd = iconv_open( cpsour, cpdisp );
    if (cd == (iconv_t) -1) {
	sprintf( errbuf, _("Can't translate from %s to %s"), cpdisp, cpsour );
	return errbuf;
    }

    for (i=128; i<=255; ++i) {
	ch = translate_character( cd, i );
	conv_input[i] = (ch == UNKNCHAR) ? i : ch;
	printable[i] = 1;
    }

    iconv_close( cd );

    return NULL;
}

void convert_to_display( char *str )
{
    while ((*str = conv_displ[ (uchar) *str ]))
	str++;
}

void convert_from_input( char *str )
{
    while ((*str = conv_input[ (uchar) *str ]))
	str++;
}
#endif /* HAVE_CHARSET */
