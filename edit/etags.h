#ifndef MC_EDIT_ETAGS_H
#define MC_EDIT_ETAGS_H 1

#define MAX_DEFINITIONS 50

struct def_hash_type {
   int filename_len;
   unsigned char *filename;
   long line;
};

int etags_set_def_hash(char *tagfile, char *start_path, char *match_func, struct def_hash_type *def_hash, int *num);

#endif
