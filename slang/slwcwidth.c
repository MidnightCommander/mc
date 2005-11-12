#include "slinclud.h"
#include <ctype.h>

#include "slang.h"
#include "_slang.h"

#define DEFINE_PSLWC_WIDTH_TABLE
#include "slwcwidth.h"
int SLwchar_wcwidth (SLwchar_Type ch)
{
   int w;
   
   SL_WIDTH_ALOOKUP(w,ch);
   return w;
}
