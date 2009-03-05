#ifndef MC_EDIT_ETAGS_H
#define MC_EDIT_ETAGS_H 1

#define MAX_DEFINITIONS 60
#define SHORT_DEF_LEN   30
#define LONG_DEF_LEN    40
#define LINE_DEF_LEN    16

typedef struct etags_hash_struct {
   int filename_len;
   unsigned char *fullpath;
   unsigned char *filename;
   unsigned char *short_define;
   long line;
} etags_hash_t;

int etags_set_def_hash(char *tagfile, char *start_path, char *match_func,  etags_hash_t *def_hash, int *num);

#endif
