#ifdef HAVE_CHARSET
#ifndef __CHARSETS_H__
#define __CHARSETS_H__

#define UNKNCHAR '\001'

#define CHARSETS_INDEX "mc.charsets"

typedef unsigned char uchar;

extern int n_codepages;

extern uchar conv_displ[256];
extern uchar conv_input[256];
extern uchar printable[256];

struct codepage_desc {
    char *id;
    char *name;
};

extern struct codepage_desc *codepages;

char *get_codepage_id( int n );
int get_codepage_index( const char *id );
int load_codepages_list(void);
char* init_printable_table( int cpdisplay );
char* init_translation_table( int cpsource, int cpdisplay );
void convert_to_display( char *str );
void convert_from_input( char *str );
void convert_string( uchar *str );

#endif /* __CHARSETS_H__ */
#endif /* HAVE_CHARSET */
