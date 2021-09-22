#include <stdio.h>

#ifdef LIB2_DEF
#error LIB2_DEF is defined!
#endif

#include "lib2.h"

#ifndef LIB2_DEF
#error LIB2_DEF is not defined!
#endif

int lib2 (void)
{
  return 0;
}
