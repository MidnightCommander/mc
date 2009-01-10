#include "slinclud.h"
#include <ctype.h>

#include "slang.h"
#include "_slang.h"

#define DEFINE_PSLWC_TOUPPER_TABLE
#include "slupper.h"


#define MODE_VARIABLE _pSLinterp_UTF8_Mode
SLwchar_Type SLwchar_toupper (SLwchar_Type ch)
{
   if (MODE_VARIABLE)
     return ch + SL_TOUPPER_LOOKUP(ch);

   return toupper (ch);
}
