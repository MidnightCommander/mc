#include "keys.h"

#define LINES 0
#define COLS  0
#define ERR   -1

enum {
  COLOR_BLACK, COLOR_RED,     COLOR_GREEN, COLOR_YELLOW,
  COLOR_BLUE,  COLOR_MAGENTA, COLOR_CYAN,  COLOR_WHITE
};

#define A_NORMAL       0
#define A_BOLD         0x40
#define A_UNDERLINE    0x40
#define A_REVERSE      0x20
#define A_BOLD_REVERSE 0x21

