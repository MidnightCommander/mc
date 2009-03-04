#ifndef MC_EDIT_ETAGS_H
#define MC_EDIT_ETAGS_H 1

#define MAX_DEFINITIONS 50
#define SHORT_DEF_LEN   30
#define LONG_DEF_LEN    40
#define LINE_DEF_LEN    16

struct etags_hash_type {
   int filename_len;
   unsigned char *fullpath;
   unsigned char *filename;
   unsigned char *short_define;
   long line;
};

int etags_set_def_hash(char *tagfile, char *start_path, char *match_func, struct etags_hash_type *def_hash, int *num);

#endif
