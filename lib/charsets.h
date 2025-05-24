/** \file charsets.h
 *  \brief Header: Text conversion from one charset to another
 */

#ifndef MC__CHARSETS_H
#define MC__CHARSETS_H

/*** typedefs(not structures) and defined constants **********************************************/

// Read one 8-bit character from buffer, Return 8-bit character.
typedef int (*get_byte_fn) (void *from, const off_t pos);
// Read one UTF-8 character from buffer. Return Unicode character of type gunchar.
typedef int (*get_utf8_fn) (void *from, const off_t pos, int *len);
/* NOTE: the 'from' argument is not of 'cont void *' type because it can be modified when buffer
 * is being read chunk by chunk (see how file is read in mcviewer) */

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct
{
    // input
    gboolean utf8;
    GIConv conv;
    get_byte_fn get_byte;
    get_utf8_fn get_utf8;

    // output
    int ch;
    int len;
    gboolean printable;
    gboolean wide;
} charset_conv_t;

/*** global variables defined in .c file *********************************************************/

extern const char *cp_display;
extern const char *cp_source;
extern GPtrArray *codepages;

/*** declarations of public functions ************************************************************/

const char *get_codepage_id (const int n);
const char *get_codepage_name (const int n);
int get_codepage_index (const char *id);
void load_codepages_list (void);
void free_codepages_list (void);
gboolean is_supported_encoding (const char *encoding);
char *init_translation_table (int cpsource, int cpdisplay);

int convert_8bit_to_display (const int c);
int convert_8bit_from_input (const int c);

unsigned char convert_utf8_to_input (const char *str);

unsigned char convert_unichar_to_8bit (const gunichar c, GIConv conv);
gunichar convert_8bit_to_unichar (const char c, GIConv conv);

gunichar convert_8bit_to_input_unichar (const char c);

gboolean charset_conv_init (charset_conv_t *conv, get_byte_fn get_byte, get_utf8_fn get_utf8);
void convert_char_to_display (charset_conv_t *conv, void *from, const off_t pos);

GString *str_nconvert_to_input (const char *str, int len);
GString *str_nconvert_to_display (const char *str, int len);

gboolean codepage_change_conv (GIConv *converter, gboolean *utf8);

/* --------------------------------------------------------------------------------------------- */
/*** inline functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static inline GString *
str_convert_to_input (const char *str)
{
    return str_nconvert_to_input (str, -1);
}

/* --------------------------------------------------------------------------------------------- */

static inline GString *
str_convert_to_display (const char *str)
{
    return str_nconvert_to_display (str, -1);
}

/* --------------------------------------------------------------------------------------------- */

#endif
