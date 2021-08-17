#include <stdio.h>

#ifdef LIB4_DEF1
#error LIB4_DEF1 is defined!
#endif

#include "lib4.h"

#ifndef LIB4_DEF1
#error LIB4_DEF1 is not defined!
#endif

int lib4 (void)
{
  return 0;
}
