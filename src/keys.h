/* Key constants as used by mc for non-curses ports */

enum {
    KEY_BACKSPACE = 400,
    KEY_END, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_A1, KEY_C1, KEY_NPAGE, KEY_PPAGE, KEY_IC,
    KEY_ENTER, KEY_DC, KEY_SCANCEL, KEY_BTAB
};

#define KEY_KP_ADD	4001
#define KEY_KP_SUBTRACT	4002
#define KEY_KP_MULTIPLY	4003


#define KEY_F(x) 1000+x
