#include <config.h>

#ifdef HAVE_CHARSET
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iconv.h>

#include "global.h"
#include "charsets.h"

int n_codepages = 0;

struct codepage_desc *codepages;

uchar conv_displ[256];
uchar conv_input[256];

int load_codepages_list(void)
{
    int result = -1;
    FILE *f;
    char *fname;
    char buf[256];
    extern char* mc_home;
    extern int display_codepage;
    char * default_codepage = NULL;

    fname = concat_dir_and_file (mc_home, CHARSETS_INDEX);
    if ( !( f = fopen( fname, "r" ) ) ) {
	fprintf (stderr, _("Warning: file %s not found\n"), fname);
	g_free (fname);
	return -1;
    }
    g_free (fname);

    for ( n_codepages=0; fgets( buf, sizeof (buf), f ); )
	if ( buf[0] != '\n' && buf[0] != '\0' && buf [0] != '#' )
	    ++n_codepages;
    rewind( f );

    codepages = g_new0 ( struct codepage_desc, n_codepages + 1 );

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
	    default_codepage = g_strdup (p);
	    continue;
	}

	codepages[n_codepages].id = g_strdup( buf  );
	codepages[n_codepages].name = g_strdup( p );
	++n_codepages;
    }

    if (default_codepage) {
	display_codepage = get_codepage_index (default_codepage);
	g_free (default_codepage);
    }

    result = n_codepages;
fail:
    fclose( f );
    return result;
}

void free_codepages_list (void)
{
    if (n_codepages > 0) {
	int i;
	for (i = 0; i < n_codepages; i++) {
	    g_free (codepages[i].id);
	    g_free (codepages[i].name);
	}
	n_codepages = 0;
	g_free (codepages);
	codepages = 0;
    }
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

static char translate_character( iconv_t cd, char c )
{
    char outbuf[4], *obuf;
    size_t ibuflen, obuflen, count;

    ICONV_CONST char *ibuf = &c; 
    obuf = outbuf;
    ibuflen = 1; obuflen = 4;

    count = iconv(cd, &ibuf, &ibuflen, &obuf, &obuflen);
    if (count >= 0 && ibuflen == 0)
	return outbuf[0];

    return UNKNCHAR;
}

char errbuf[255];

/*
 * FIXME: This assumes that ASCII is always the first encoding
 * in mc.charsets
 */
#define CP_ASCII 0

char* init_translation_table( int cpsource, int cpdisplay )
{
    int i;
    iconv_t cd;
    char *cpsour, *cpdisp;

    /* Fill inpit <-> display tables */

    if (cpsource < 0 || cpdisplay < 0 || cpsource == cpdisplay) {
	for (i=0; i<=255; ++i) {
	    conv_displ[i] = i;
	    conv_input[i] = i;
	}
	return NULL;
    }

    for (i=0; i<=127; ++i) {
	conv_displ[i] = i;
	conv_input[i] = i;
    }

    cpsour = codepages[ cpsource ].id;
    cpdisp = codepages[ cpdisplay ].id;

    /* display <- inpit table */

    cd = iconv_open( cpdisp, cpsour );
    if (cd == (iconv_t) -1) {
	sprintf( errbuf, _("Cannot translate from %s to %s"), cpsour, cpdisp );
	return errbuf;
    }

    for (i=128; i<=255; ++i)
	conv_displ[i] = translate_character( cd, i );

    iconv_close( cd );

    /* inpit <- display table */

    cd = iconv_open( cpsour, cpdisp );
    if (cd == (iconv_t) -1) {
	sprintf( errbuf, _("Cannot translate from %s to %s"), cpdisp, cpsour );
	return errbuf;
    }

    for (i=128; i<=255; ++i) {
	uchar ch;
	ch = translate_character( cd, i );
	conv_input[i] = (ch == UNKNCHAR) ? i : ch;
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
