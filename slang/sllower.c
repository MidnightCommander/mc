#include "slinclud.h"
#include <ctype.h>

#include "slang.h"
#include "_slang.h"

#define DEFINE_PSLWC_TOLOWER_TABLE
#include "sllower.h"

#define MODE_VARIABLE _pSLinterp_UTF8_Mode
SLwchar_Type SLwchar_tolower (SLwchar_Type ch)
{
   if (MODE_VARIABLE)
     return ch + SL_TOLOWER_LOOKUP(ch);
   
   return tolower(ch);
}
