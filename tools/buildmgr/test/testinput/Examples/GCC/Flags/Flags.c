#include <stdio.h>

#include DEF_RTE_Components
#include CMSIS_device_header

int main (void) {
#ifndef DEF_TARGET_CC
#error "DEF_TARGET_CC is not defined!"
#endif
#ifndef DEF_CC
#error "DEF_CC is not defined!"
#endif

  if (DEF_CC == 9999) {
    printf("Test successful\n\r");
  }
  return 0;
}

