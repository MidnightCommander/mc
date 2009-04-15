#ifndef MC_STRUTIL_H
#define MC_STRUTIL_H

/* Header file for strutil.c, strutilascii.c, strutil8bit.c, strutilutf8.c.
 * There are two sort of functions:
 * 1. functions for working with growing strings and conversion strings between
 *    different encodings.
 *    (implemented directly in strutil.c)
 * 2. functions, that hide differences between encodings derived from ASCII.
 *    (implemented separately in strutilascii.c, strutil8bit.c, strutilutf8.c)
 * documentation is made for UTF-8 version of functions.
 */
 
/* invalid strings
 * function, that works with invalid strings are marked with "I" 
 * in documentation
 * invalid bytes of string are handled as one byte characters with width 1, they
 * are displayed as questionmarks, I-maked comparing functions try to keep 
 * the original value of these bytes.
 */
 
/* combining characters
 * displaynig: all handled as zero with characters, expect combing character 
 * at the begin of string, this character has with one (space add before), 
 * so str_term_width is not good for computing width of singles characters 
 * (never return zero, expect emtpy string)
 * for compatibility are strings composed before displaynig
 * comparing: comparing decompose all string before comparing, n-compare 
 * functions do not work as is usual, because same strings do not have to be 
 * same length in UTF-8. So they return 0 if one string is prefix of the other 
 * one. 
 * str_prefix is used to determine, how many characters from one string are 
 * prefix in second string. However, str_prefix return number of characters in
 * decompose form. (used in do_search (screen.c))
 */
#include <iconv.h>

/* errors for conversion function:
 * problem means, that not every characters was successfully converted (They are
 * replaced with questionmark). So is impossible convert string back. 
 * failure means, that conversion is not possible (example: wrong encoding 
 * of input string)
 */
#define ESTR_PROBLEM    1
#define ESTR_FAILURE    2

/* constanst originally from screen.c
 * used for alignment strings on terminal
 */
#define J_LEFT 		0x01
#define J_RIGHT		0x02
#define J_CENTER	0x03
// if there is enough space for string on terminal, string is centered
// otherwise is aligned to left
#define J_CENTER_LEFT	0x04

#define IS_FIT(x)	((x) & 0x0010)
#define MAKE_FIT(x)	((x) | 0x0010)
#define HIDE_FIT(x)	((x) & 0x000f)

// fit alignment, if string is to long, is truncated with '~'
#define J_LEFT_FIT	   0x11
#define J_RIGHT_FIT	   0x12
#define J_CENTER_FIT	   0x13
#define J_CENTER_LEFT_FIT  0x14

// redefinition of iconv_t, so is not needed include iconv.h in other files.
#define INVALID_CONV ((iconv_t) (-1))

// standard convertors 
extern GIConv str_cnv_to_term;
extern GIConv str_cnv_from_term;
// from terminal encoding to terminal encoding
extern GIConv str_cnv_not_convert;

// all functions in str_class must be defined for every encoding
struct str_class {
    int (*vfs_convert_to) (GIConv coder, const char *string, 
                        int size, GString *buffer);           //I
    void (*insert_replace_char) (GString *buffer);
    int (*is_valid_string) (const char *);                              //I
    int (*is_valid_char) (const char *, size_t);                        //I
    void (*cnext_char) (const char **);
    void (*cprev_char) (const char **);
    void (*cnext_char_safe) (const char **);                            //I
    void (*cprev_char_safe) (const char **);                            //I
    int (*cnext_noncomb_char) (const char **text);                      //I
    int (*cprev_noncomb_char) (const char **text, const char *begin);   //I
    int (*isspace) (const char *);                                      //I
    int (*ispunct) (const char *);                                      //I
    int (*isalnum) (const char *);                                      //I
    int (*isdigit) (const char *);                                      //I
    int (*isprint) (const char *);                                      //I
    int (*iscombiningmark) (const char *);                              //I
    int (*length) (const char *);                                       //I
    int (*length2) (const char *, int);                                 //I
    int (*length_noncomb) (const char *);                               //I
    int (*toupper) (const char *, char **, size_t *);
    int (*tolower) (const char *, char **, size_t *);
    void (*fix_string) (char *);                                        //I
    const char *(*term_form) (const char *);                            //I
    const char *(*fit_to_term) (const char *, int, int);                //I
    const char *(*term_trim) (const char *text, int width);             //I
    void (*msg_term_size) (const char *, int *, int *);                 //I
    const char *(*term_substring) (const char *, int, int);             //I
    int (*term_width1) (const char *);                                  //I
    int (*term_width2) (const char *, size_t);                          //I
    int (*term_char_width) (const char *);                              //I
    const char *(*trunc) (const char *, int);                           //I
    int (*offset_to_pos) (const char *, size_t);                        //I
    int (*column_to_pos) (const char *, size_t);                        //I
    char *(*create_search_needle) (const char *, int);
    void (*release_search_needle) (char *, int);
    const char *(*search_first) (const char *, const char *, int);
    const char *(*search_last) (const char *, const char *, int);
    int (*compare) (const char *, const char *);                        //I
    int (*ncompare) (const char *, const char *);                       //I
    int (*casecmp) (const char *, const char *);                        //I
    int (*ncasecmp) (const char *, const char *);                       //I
    int (*prefix) (const char *, const char *);                         //I
    int (*caseprefix) (const char *, const char *);                     //I
    char *(*create_key) (const char *text, int case_sen);               //I
    char *(*create_key_for_filename) (const char *text, int case_sen);  //I
    int (*key_collate) (const char *t1, const char *t2, int case_sen);  //I
    void (*release_key) (char *key, int case_sen);                      //I
};

struct str_class str_utf8_init ();
struct str_class str_8bit_init ();
struct str_class str_ascii_init ();

/* create convertor from "from_enc" to terminal encoding
 * if "from_enc" is not supported return INVALID_CONV 
 */
GIConv str_crt_conv_from (const char *);

/* create convertor from terminal encoding to "to_enc"
 * if "to_enc" is not supported return INVALID_CONV 
 */
GIConv str_crt_conv_to (const char *);

/* close convertor, do not close str_cnv_to_term, str_cnv_from_term, 
 * str_cnv_not_convert 
 */
void str_close_conv (GIConv);

/* return on of not used buffers (.used == 0) or create new
 * returned buffer has set .used to 1
 */

/* convert string using coder, result of conversion is appended at end of buffer
 * return 0 if there was no problem. 
 * otherwise return  ESTR_PROBLEM or ESTR_FAILURE
 */ 
int str_convert (GIConv, char *, GString *);

int str_nconvert (GIConv, char *, int, GString *);

/* return only 0 or ESTR_FAILURE, because vfs must be able to convert result to
 * original string. (so no replace with questionmark)
 * if coder is str_cnv_from_term or str_cnv_not_convert, string is only copied,
 * so is possible to show file, that is not valid in terminal encoding
 */                 
int str_vfs_convert_from (GIConv, char *, GString *);

/* if coder is str_cnv_to_term or str_cnv_not_convert, string is only copied,
 * does replace with questionmark 
 * I
 */
int str_vfs_convert_to (GIConv, const char *, int, GString *);

/* printf functin for str_buffer, append result of printf at the end of buffer
 */
void
str_printf (GString *, const char *, ...);

/* add standard replacement character in terminal encoding
 */
void str_insert_replace_char (GString *);

/* init strings and set terminal encoding,
 * if is termenc NULL, detect terminal encoding
 * create all str_cnv_* and set functions for terminal encoding
 */
void str_init_strings (const char *termenc);

/* free all str_buffer and all str_cnv_*
 */
void str_uninit_strings ();

/* try convert characters in ch to output using conv
 * ch_size is size of ch, can by (size_t)(-1) (-1 only for ASCII 
 *     compatible encoding, for other must be set)
 * return 0 if conversion was successfully, ESTR_PROBLEM if ch contains only 
 * part of characters, ESTR_FAILURE if conversion is not possible
 */
int str_translate_char (GIConv conv, char *ch, size_t ch_size, 
                        char *output, size_t out_size);

/* test, if text is valid in terminal encoding
 * I
 */
int str_is_valid_string (const char *text);

/* test, if first char of ch is valid
 * size, how many bytes characters occupied, could be (size_t)(-1)
 * return 1 if it is valid, -1 if it is invalid or -2 if it is only part of 
 * multibyte character 
 * I
 */
int str_is_valid_char (const char *ch, size_t size); 

/* return next characters after text, do not call on the end of string
 */
char *str_get_next_char (char *text);
const char *str_cget_next_char (const char *text);

/* return previous characters before text, do not call on the start of strings
 */
char *str_get_prev_char (char *text);
const char *str_cget_prev_char (const char *text);

/* set text to next characters, do not call on the end of string
 */
void str_next_char (char **text);
void str_cnext_char (const char **text);

/* set text to previous characters, do not call on the start of strings
 */
void str_prev_char (char **text);
void str_cprev_char (const char **text);

/* return next characters after text, do not call on the end of string
 * works with invalid string 
 * I
 */
char *str_get_next_char_safe (char *text);
const char *str_cget_next_char_safe (const char *text);

/* return previous characters before text, do not call on the start of strings
 * works with invalid string 
 * I
 */
char *str_get_prev_char_safe (char *text);
const char *str_cget_prev_char_safe (const char *text);

/* set text to next characters, do not call on the end of string
 * works with invalid string 
 * I
 */
void str_next_char_safe (char **text);
void str_cnext_char_safe (const char **text);

/* set text to previous characters, do not call on the start of strings
 * works with invalid string 
 * I
 */
void str_prev_char_safe (char **text);
void str_cprev_char_safe (const char **text);

/* set text to next noncombining characters, check the end of text
 * return how many characters was skipped
 * works with invalid string 
 * I
 */
int str_next_noncomb_char (char **text);
int str_cnext_noncomb_char (const char **text);

/* set text to previous noncombining characters, search stop at begin 
 * return how many characters was skipped
 * works with invalid string 
 * I
 */
int str_prev_noncomb_char (char **text, const char *begin);
int str_cprev_noncomb_char (const char **text, const char *begin);

/* if first characters in ch is space, tabulator  or new lines
 * I
 */
int str_isspace (const char *ch);

/* if first characters in ch is punctuation or symbol
 * I
 */
int str_ispunct (const char *ch);

/* if first characters in ch is alphanum
 * I
 */
int str_isalnum (const char *ch);

/* if first characters in ch is digit
 * I
 */
int str_isdigit (const char *ch);

/* if first characters in ch is printable
 * I
 */
int str_isprint (const char *ch);

/* if first characters in ch is a combining mark (only in utf-8)
 * combining makrs are assumed to be zero width 
 * I
 */
int str_iscombiningmark (const char *ch);

/* write lower from of fisrt characters in ch into out
 * decrase remain by size of returned characters
 * if out is not big enough, do nothing
 */
int str_toupper (const char *ch, char **out, size_t *remain);

/* write upper from of fisrt characters in ch into out
 * decrase remain by size of returned characters
 * if out is not big enough, do nothing
 */
int str_tolower (const char *ch, char **out, size_t *remain);

/* return length of text in characters
 * I
 */
int str_length (const char* text);

/* return length of text in characters, limit to size
 * I
 */
int str_length2 (const char* text, int size);

/* return length of text in characters, count only noncombining characters
 * I
 */
int str_length_noncomb (const char* text);

/* replace all invalid characters in text with questionmark
 * after return, text is valid string in terminal encoding
 * I
 */
void str_fix_string (char* text);

/* replace all invalid characters in text with questionmark
 * replace all unprintable characters with '.'
 * return static allocated string, "text" is not changed
 * returned string do not need to be freed
 * I
 */
const char *str_term_form (const char *text); 

/* like str_term_form, but text can be alignment to width
 * alignment is specified in just_mode (J_LEFT, J_LEFT_FIT, ...)
 * result is completed with spaces to width
 * I
 */
const char *str_fit_to_term (const char *text, int width, int just_mode);

/* like str_term_form, but when text is wider than width, three dots are
 * inserted at begin and result is completed with suffix of text
 * no additional spaces are inserted
 * I
 */
const char *str_term_trim (const char *text, int width);

/* return how many lines and columns will text occupy on terminal
 * I
 */
void str_msg_term_size (const char *text, int *lines, int *columns);

/* like str_term_form, but return only specified substring
 * start - column (position) on terminal, where substring begin
 * result is completed with spaces to width
 * I
 */ 
const char *str_term_substring (const char *text, int start, int width);

/* return width, that will be text occupied on terminal
 * I
 */
int str_term_width1 (const char *text);

/* return width, that will be text occupied on terminal
 * text is limited by length in characters
 * I
 */
int str_term_width2 (const char *text, size_t length);

/* return width, that will be character occupied on terminal
 * combining characters are always zero width
 * I
 */
int str_term_char_width (const char *text);

/* convert position in characters to position in bytes 
 * I
 */
int str_offset_to_pos (const char* text, size_t length);

/* convert position on terminal to position in characters
 * I
 */
int str_column_to_pos (const char *text, size_t pos);

/* like str_fit_to_term width just_mode = J_LEFT_FIT, 
 * but do not insert additional spaces
 * I
 */
const char *str_trunc (const char *text, int width);

/* create needle, that will be searched in str_search_fist/last,
 * so needle can be reused
 * in UTF-8 return normalized form of needle
 */
char *str_create_search_needle (const char *needle, int case_sen);

/* free needle returned by str_create_search_needle
 */
void str_release_search_needle (char *needle, int case_sen);

/* search for first occurrence of search in text
 */
const char *str_search_first (const char *text, const char *needle, int case_sen);

/* search for last occurrence of search in text
 */
const char *str_search_last (const char *text, const char *needle, int case_sen);

/* case sensitive compare two strings
 * I
 */
int str_compare (const char *t1, const char *t2);

/* case sensitive compare two strings
 * if one string is prefix of the other string, return 0
 * I
 */
int str_ncompare (const char *t1, const char *t2);

/* case insensitive compare two strings
 * I
 */
int str_casecmp (const char *t1, const char *t2);

/* case insensitive compare two strings
 * if one string is prefix of the other string, return 0
 * I
 */
int str_ncasecmp (const char *t1, const char *t2);

/* return, how many bytes are are same from start in text and prefix
 * both strings are decomposed befor comapring and return value is counted
 * in decomposed form, too. caling with prefix, prefix, you get size in bytes
 * of prefix in decomposed form,
 * I
 */
int str_prefix (const char *text, const char *prefix);

/* case insensitive version of str_prefix
 * I
 */
int str_caseprefix (const char *text, const char *prefix);

/* create a key that is used by str_key_collate
 * I
 */
char *str_create_key (const char *text, int case_sen);

/* create a key that is used by str_key_collate
 * should aware dot '.' in text
 * I
 */
char *str_create_key_for_filename (const char *text, int case_sen);

/* compare two string using LC_COLLATE, if is possible
 * if case_sen is set, comparing is case sensitive,
 * case_sen must be same for str_create_key, str_key_collate and str_release_key
 * I
 */
int str_key_collate (const char *t1, const char *t2, int case_sen);

/* release_key created by str_create_key, only rigth way to release key
 * I
 */
void str_release_key (char *key, int case_sen);

/* return 1 if codeset_name is utf8 or utf-8 
 * I
 */
int str_isutf8 (char *codeset_name);

#endif

