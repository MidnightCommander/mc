#ifndef __CHARSETS_H__
#define __CHARSETS_H__

#ifdef HAVE_CHARSET

#define UNKNCHAR '\001'

#define CHARSETS_INDEX "mc.charsets"

extern int n_codepages;

extern unsigned char conv_displ[256];
extern unsigned char conv_input[256];
extern unsigned char printable[256];

struct codepage_desc {
    char *id;
    char *name;
};

extern struct codepage_desc *codepages;

char *get_codepage_id (int n);
int get_codepage_index (const char *id);
int load_codepages_list (void);
void free_codepages_list (void);
char *init_translation_table (int cpsource, int cpdisplay);
void convert_to_display (char *str);
void convert_from_input (char *str);
void convert_string (unsigned char *str);

#else /* !HAVE_CHARSET */
#define convert_to_display(x) do {} while (0)
#define convert_from_input(x) do {} while (0)
#endif /* HAVE_CHARSET */

#endif /* __CHARSETS_H__ */
