#define MAX_DEFINITIONS 50

struct def_hash_type {
   int filename_len;
   unsigned char *filename;
   long line;
};
long get_pos_from(char *str);
int set_def_hash(char *tagfile, char *start_path, char *match_func, struct def_hash_type *def_hash, int *num);
