/** \file charsets.h
 *  \brief Header: Text conversion from one charset to another
 */

#ifndef MC__CHARSETS_H
#define MC__CHARSETS_H

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

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

GString *str_nconvert_to_input (const char *str, int len);
GString *str_nconvert_to_display (const char *str, int len);

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
