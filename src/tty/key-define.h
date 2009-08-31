#ifndef MC_KEY_DEFINE_H
#define MC_KEY_DEFINE_H

typedef struct {
    int code;
    const char *seq;
    int action;
} key_define_t;

static key_define_t mc_default_keys [] = {
    { ESC_CHAR, ESC_STR, MCKEY_ESCAPE },
    { ESC_CHAR, ESC_STR ESC_STR, MCKEY_NOACTION },
    { 0, NULL, MCKEY_NOACTION },
};

/* Broken terminfo and termcap databases on xterminals */
static key_define_t xterm_key_defines [] = {
    { KEY_F(1),   ESC_STR "OP",   MCKEY_NOACTION },
    { KEY_F(2),   ESC_STR "OQ",   MCKEY_NOACTION },
    { KEY_F(3),   ESC_STR "OR",   MCKEY_NOACTION },
    { KEY_F(4),   ESC_STR "OS",   MCKEY_NOACTION },
    { KEY_F(1),   ESC_STR "[11~", MCKEY_NOACTION },
    { KEY_F(2),   ESC_STR "[12~", MCKEY_NOACTION },
    { KEY_F(3),   ESC_STR "[13~", MCKEY_NOACTION },
    { KEY_F(4),   ESC_STR "[14~", MCKEY_NOACTION },
    { KEY_F(5),   ESC_STR "[15~", MCKEY_NOACTION },
    { KEY_F(6),   ESC_STR "[17~", MCKEY_NOACTION },
    { KEY_F(7),   ESC_STR "[18~", MCKEY_NOACTION },
    { KEY_F(8),   ESC_STR "[19~", MCKEY_NOACTION },
    { KEY_F(9),   ESC_STR "[20~", MCKEY_NOACTION },
    { KEY_F(10),  ESC_STR "[21~", MCKEY_NOACTION },

    /* old xterm Shift-arrows */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "O2A",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "O2B",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "O2C",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "O2D",   MCKEY_NOACTION },

    /* new xterm Shift-arrows */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[1;2A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[1;2B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[1;2C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[1;2D", MCKEY_NOACTION },

    /* more xterm keys with modifiers */
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_IC,    ESC_STR "[2;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DC,    ESC_STR "[3;5~", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_HOME,  ESC_STR "[1;5H", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_END,   ESC_STR "[1;5F", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "[1;2H", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "[1;2F", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "[1;5A", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "[1;5B", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "[1;5C", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "[1;5D", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_IC,    ESC_STR "[2;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC,    ESC_STR "[3;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "[1;6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "[1;6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[1;6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "[1;6D", MCKEY_NOACTION },

    /* putty */
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "[[1;6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "[[1;6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[[1;6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "[[1;6D", MCKEY_NOACTION },

    /* putty alt-arrow keys */
    /* removed as source esc esc esc trouble */
    /*
    { KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "OA", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "OB", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "OC", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "OD", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[5~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[6~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[4~", MCKEY_NOACTION },

    { KEY_M_CTRL | KEY_M_ALT | KEY_UP,    ESC_STR ESC_STR "[1;2A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_DOWN,  ESC_STR ESC_STR "[1;2B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR ESC_STR "[1;2C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_LEFT,  ESC_STR ESC_STR "[1;2D", MCKEY_NOACTION },

    { KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR ESC_STR "[[5;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR ESC_STR "[[6;5~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_HOME,  ESC_STR ESC_STR "[1;5H", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_END,   ESC_STR ESC_STR "[1;5F", MCKEY_NOACTION },
    */
    /* xterm alt-arrow keys */
    { KEY_M_ALT | KEY_UP,    ESC_STR "[1;3A", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_DOWN,  ESC_STR "[1;3B", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;3C", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_LEFT,  ESC_STR "[1;3D", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;3~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;3~", MCKEY_NOACTION },
    { KEY_M_ALT | KEY_HOME,  ESC_STR "[1~",   MCKEY_NOACTION },
    { KEY_M_ALT | KEY_END,   ESC_STR "[4~",   MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_UP,    ESC_STR "[1;7A", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_DOWN,  ESC_STR "[1;7B", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_RIGHT, ESC_STR "[1;7C", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_LEFT,  ESC_STR "[1;7D", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_PPAGE, ESC_STR "[5;7~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_NPAGE, ESC_STR "[6;7~", MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_HOME,  ESC_STR "OH",    MCKEY_NOACTION },
    { KEY_M_CTRL | KEY_M_ALT | KEY_END,   ESC_STR "OF",    MCKEY_NOACTION },

    /* rxvt keys with modifiers */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[a",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[b",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[c",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[d",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "Oa",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "Ob",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "Oc",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "Od",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_PPAGE, ESC_STR "[5^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_NPAGE, ESC_STR "[6^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_HOME,  ESC_STR "[7^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_END,   ESC_STR "[8^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "[7$", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "[8$", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_IC,    ESC_STR "[2^", MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DC,    ESC_STR "[3^", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DC,    ESC_STR "[3$", MCKEY_NOACTION },

    /* konsole keys with modifiers */
    { KEY_M_SHIFT | KEY_HOME,  ESC_STR "O2H",   MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_END,   ESC_STR "O2F",   MCKEY_NOACTION },

    /* gnome-terminal */
    { KEY_M_SHIFT | KEY_UP,    ESC_STR "[2A",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_DOWN,  ESC_STR "[2B",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_RIGHT, ESC_STR "[2C",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_LEFT,  ESC_STR "[2D",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "[5A",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "[5B",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "[5C",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "[5D",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "[6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "[6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "[6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "[6D", MCKEY_NOACTION },

    /* gnome-terminal - application mode */
    { KEY_M_CTRL  | KEY_UP,    ESC_STR "O5A",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_DOWN,  ESC_STR "O5B",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_RIGHT, ESC_STR "O5C",  MCKEY_NOACTION },
    { KEY_M_CTRL  | KEY_LEFT,  ESC_STR "O5D",  MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_UP,    ESC_STR "O6A", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_DOWN,  ESC_STR "O6B", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_RIGHT, ESC_STR "O6C", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_M_CTRL | KEY_LEFT,  ESC_STR "O6D", MCKEY_NOACTION },

    /* iTerm */
    { KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[5;2~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[6;2~", MCKEY_NOACTION },

    /* putty */
    { KEY_M_SHIFT | KEY_PPAGE, ESC_STR "[[5;53~", MCKEY_NOACTION },
    { KEY_M_SHIFT | KEY_NPAGE, ESC_STR "[[6;53~", MCKEY_NOACTION },

    /* keypad keys */
    { KEY_IC,                  ESC_STR "Op",  MCKEY_NOACTION },
    { KEY_DC,                  ESC_STR "On",  MCKEY_NOACTION },
    { '/',                     ESC_STR "Oo",  MCKEY_NOACTION },
    { '\n',                    ESC_STR "OM",  MCKEY_NOACTION },

    { 0, NULL, MCKEY_NOACTION },
};

/* qansi-m terminals have a much more key combinatios,
   which are undefined in termcap/terminfo */
static key_define_t qansi_key_defines[] =
{
    /* qansi-m terminal */
    {KEY_M_CTRL | KEY_NPAGE,   ESC_STR "[u", MCKEY_NOACTION}, /* Ctrl-PgDown */
    {KEY_M_CTRL | KEY_PPAGE,   ESC_STR "[v", MCKEY_NOACTION}, /* Ctrl-PgUp   */
    {KEY_M_CTRL | KEY_HOME,    ESC_STR "[h", MCKEY_NOACTION}, /* Ctrl-Home   */
    {KEY_M_CTRL | KEY_END,     ESC_STR "[y", MCKEY_NOACTION}, /* Ctrl-End    */
    {KEY_M_CTRL | KEY_IC,      ESC_STR "[`", MCKEY_NOACTION}, /* Ctrl-Insert */
    {KEY_M_CTRL | KEY_DC,      ESC_STR "[p", MCKEY_NOACTION}, /* Ctrl-Delete */
    {KEY_M_CTRL | KEY_LEFT,    ESC_STR "[d", MCKEY_NOACTION}, /* Ctrl-Left   */
    {KEY_M_CTRL | KEY_RIGHT,   ESC_STR "[c", MCKEY_NOACTION}, /* Ctrl-Right  */
    {KEY_M_CTRL | KEY_DOWN,    ESC_STR "[b", MCKEY_NOACTION}, /* Ctrl-Down   */
    {KEY_M_CTRL | KEY_UP,      ESC_STR "[a", MCKEY_NOACTION}, /* Ctrl-Up     */
    {KEY_M_CTRL | KEY_KP_ADD,  ESC_STR "[s", MCKEY_NOACTION}, /* Ctrl-Gr-Plus */
    {KEY_M_CTRL | KEY_KP_SUBTRACT, ESC_STR "[t", MCKEY_NOACTION}, /* Ctrl-Gr-Minus */
    {KEY_M_CTRL  | '\t',       ESC_STR "[z", MCKEY_NOACTION}, /* Ctrl-Tab    */
    {KEY_M_SHIFT | '\t',       ESC_STR "[Z", MCKEY_NOACTION}, /* Shift-Tab   */
    {KEY_M_CTRL | KEY_F(1),    ESC_STR "[1~", MCKEY_NOACTION}, /* Ctrl-F1    */
    {KEY_M_CTRL | KEY_F(2),    ESC_STR "[2~", MCKEY_NOACTION}, /* Ctrl-F2    */
    {KEY_M_CTRL | KEY_F(3),    ESC_STR "[3~", MCKEY_NOACTION}, /* Ctrl-F3    */
    {KEY_M_CTRL | KEY_F(4),    ESC_STR "[4~", MCKEY_NOACTION}, /* Ctrl-F4    */
    {KEY_M_CTRL | KEY_F(5),    ESC_STR "[5~", MCKEY_NOACTION}, /* Ctrl-F5    */
    {KEY_M_CTRL | KEY_F(6),    ESC_STR "[6~", MCKEY_NOACTION}, /* Ctrl-F6    */
    {KEY_M_CTRL | KEY_F(7),    ESC_STR "[7~", MCKEY_NOACTION}, /* Ctrl-F7    */
    {KEY_M_CTRL | KEY_F(8),    ESC_STR "[8~", MCKEY_NOACTION}, /* Ctrl-F8    */
    {KEY_M_CTRL | KEY_F(9),    ESC_STR "[9~", MCKEY_NOACTION}, /* Ctrl-F9    */
    {KEY_M_CTRL | KEY_F(10),   ESC_STR "[10~", MCKEY_NOACTION}, /* Ctrl-F10  */
    {KEY_M_CTRL | KEY_F(11),   ESC_STR "[11~", MCKEY_NOACTION}, /* Ctrl-F11  */
    {KEY_M_CTRL | KEY_F(12),   ESC_STR "[12~", MCKEY_NOACTION}, /* Ctrl-F12  */
    {KEY_M_ALT  | KEY_F(1),    ESC_STR "[17~", MCKEY_NOACTION}, /* Alt-F1    */
    {KEY_M_ALT  | KEY_F(2),    ESC_STR "[18~", MCKEY_NOACTION}, /* Alt-F2    */
    {KEY_M_ALT  | KEY_F(3),    ESC_STR "[19~", MCKEY_NOACTION}, /* Alt-F3    */
    {KEY_M_ALT  | KEY_F(4),    ESC_STR "[20~", MCKEY_NOACTION}, /* Alt-F4    */
    {KEY_M_ALT  | KEY_F(5),    ESC_STR "[21~", MCKEY_NOACTION}, /* Alt-F5    */
    {KEY_M_ALT  | KEY_F(6),    ESC_STR "[22~", MCKEY_NOACTION}, /* Alt-F6    */
    {KEY_M_ALT  | KEY_F(7),    ESC_STR "[23~", MCKEY_NOACTION}, /* Alt-F7    */
    {KEY_M_ALT  | KEY_F(8),    ESC_STR "[24~", MCKEY_NOACTION}, /* Alt-F8    */
    {KEY_M_ALT  | KEY_F(9),    ESC_STR "[25~", MCKEY_NOACTION}, /* Alt-F9    */
    {KEY_M_ALT  | KEY_F(10),   ESC_STR "[26~", MCKEY_NOACTION}, /* Alt-F10   */
    {KEY_M_ALT  | KEY_F(11),   ESC_STR "[27~", MCKEY_NOACTION}, /* Alt-F11   */
    {KEY_M_ALT  | KEY_F(12),   ESC_STR "[28~", MCKEY_NOACTION}, /* Alt-F12   */
    {KEY_M_ALT  | 'a',         ESC_STR "Na",   MCKEY_NOACTION}, /* Alt-a     */
    {KEY_M_ALT  | 'b',         ESC_STR "Nb",   MCKEY_NOACTION}, /* Alt-b     */
    {KEY_M_ALT  | 'c',         ESC_STR "Nc",   MCKEY_NOACTION}, /* Alt-c     */
    {KEY_M_ALT  | 'd',         ESC_STR "Nd",   MCKEY_NOACTION}, /* Alt-d     */
    {KEY_M_ALT  | 'e',         ESC_STR "Ne",   MCKEY_NOACTION}, /* Alt-e     */
    {KEY_M_ALT  | 'f',         ESC_STR "Nf",   MCKEY_NOACTION}, /* Alt-f     */
    {KEY_M_ALT  | 'g',         ESC_STR "Ng",   MCKEY_NOACTION}, /* Alt-g     */
    {KEY_M_ALT  | 'i',         ESC_STR "Ni",   MCKEY_NOACTION}, /* Alt-i     */
    {KEY_M_ALT  | 'j',         ESC_STR "Nj",   MCKEY_NOACTION}, /* Alt-j     */
    {KEY_M_ALT  | 'k',         ESC_STR "Nk",   MCKEY_NOACTION}, /* Alt-k     */
    {KEY_M_ALT  | 'l',         ESC_STR "Nl",   MCKEY_NOACTION}, /* Alt-l     */
    {KEY_M_ALT  | 'm',         ESC_STR "Nm",   MCKEY_NOACTION}, /* Alt-m     */
    {KEY_M_ALT  | 'n',         ESC_STR "Nn",   MCKEY_NOACTION}, /* Alt-n     */
    {KEY_M_ALT  | 'o',         ESC_STR "No",   MCKEY_NOACTION}, /* Alt-o     */
    {KEY_M_ALT  | 'p',         ESC_STR "Np",   MCKEY_NOACTION}, /* Alt-p     */
    {KEY_M_ALT  | 'q',         ESC_STR "Nq",   MCKEY_NOACTION}, /* Alt-r     */
    {KEY_M_ALT  | 's',         ESC_STR "Ns",   MCKEY_NOACTION}, /* Alt-s     */
    {KEY_M_ALT  | 't',         ESC_STR "Nt",   MCKEY_NOACTION}, /* Alt-t     */
    {KEY_M_ALT  | 'u',         ESC_STR "Nu",   MCKEY_NOACTION}, /* Alt-u     */
    {KEY_M_ALT  | 'v',         ESC_STR "Nv",   MCKEY_NOACTION}, /* Alt-v     */
    {KEY_M_ALT  | 'w',         ESC_STR "Nw",   MCKEY_NOACTION}, /* Alt-w     */
    {KEY_M_ALT  | 'x',         ESC_STR "Nx",   MCKEY_NOACTION}, /* Alt-x     */
    {KEY_M_ALT  | 'y',         ESC_STR "Ny",   MCKEY_NOACTION}, /* Alt-y     */
    {KEY_M_ALT  | 'z',         ESC_STR "Nz",   MCKEY_NOACTION}, /* Alt-z     */
    {KEY_KP_SUBTRACT,          ESC_STR "[S",   MCKEY_NOACTION}, /* Gr-Minus  */
    {KEY_KP_ADD,               ESC_STR "[T",   MCKEY_NOACTION}, /* Gr-Plus   */
    {0, NULL, MCKEY_NOACTION},
};

#endif				/* MC_KEY_H */
