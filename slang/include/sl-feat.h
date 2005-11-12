/* Setting this to one permits strings with embedded color changing commands.
 * This is experimental.
 */
#define SLSMG_HAS_EMBEDDED_ESCAPE	1

/* Setting this to 1 enables automatic support for associative arrays.
 * If this is set to 0, an application must explicitly enable associative
 * array support via SLang_init_slassoc.
 */
#define SLANG_HAS_ASSOC_ARRAYS		1

#define SLANG_HAS_COMPLEX		1
#define SLANG_HAS_FLOAT			1

/* This is the old space-speed trade off.  To reduce memory usage and code
 * size, set this to zero.
 */
/* #define SLANG_OPTIMIZE_FOR_SPEED	0 */ 
#define SLANG_OPTIMIZE_FOR_SPEED	2

#define SLANG_USE_INLINE_CODE		1

/* Add extra information for tracking down errors. */
#define SLANG_HAS_DEBUG_CODE		1

/* Experimental: Support for examining call frames */
#define SLANG_HAS_DEBUGGER_SUPPORT	1

/* Allow optimizations based upon the __tmp operator. */
#define SLANG_USE_TMP_OPTIMIZATION	1

/* Setting this to one will map 8 bit vtxxx terminals to 7 bit.  Terminals
 * such as the vt320 can be set up to output the two-character escape sequence
 * encoded as 'ESC [' as single character.  Setting this variable to 1 will
 * insert code to map such characters to the 7 bit equivalent.
 * This affects just input characters in the range 128-160 on non PC
 * systems.
 */
#if defined(VMS) || defined(AMIGA)
# define SLANG_MAP_VTXXX_8BIT	1
#else
# define SLANG_MAP_VTXXX_8BIT	0
#endif

/* Add support for color terminals that cannot do background color erases
 * Such terminals are poorly designed and are slowly disappearing but they
 * are still quite common.  For example, screen is one of them!
 * 
 * This is experimental.  In particular, it is not known to work if 
 * KANJI suupport is enabled.
 */
#if !defined(IBMPC_SYSTEM)
# define SLTT_HAS_NON_BCE_SUPPORT	1
#else
# define SLTT_HAS_NON_BCE_SUPPORT	0
#endif

/* If you want slang to assume that an xterm always has the background color
 * erase feature, then set this to 1.  Otherwise, it will check the terminfo
 * database.  This may or may not be a good idea since most good color xterms
 * support bce but many terminfo systems do not support it.
 */
#define SLTT_XTERM_ALWAYS_BCE		0
  
/* Set this to 1 to enable Kanji support.  See above comment. */
#define SLANG_HAS_KANJI_SUPPORT		0

#define SLANG_HAS_SIGNALS		1

/* Enable this if you want beginning-of-statement and end-of-statement 
 * callbacks.  This allows the creation of profilers, stack-checkers, etc.
 * SLANG_HAS_DEBUG_CODE must be enabled for this to work.
 */
#define SLANG_HAS_BOSEOS	SLANG_HAS_DEBUG_CODE
