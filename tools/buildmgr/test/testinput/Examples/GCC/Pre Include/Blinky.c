#include <stdio.h>

#include "cmsis_os2.h"

#include "RTE_Components.h"
#include CMSIS_device_header

#include "header.h"
#ifndef HEADER1
#error "HEADER1 is not defined"
#endif
#ifdef HEADER2
#error "HEADER2 is defined"
#endif

#include "include2/header.h"
#ifndef HEADER2
#error "HEADER2 is not defined"
#endif
#ifdef HEADER1
#error "HEADER1 is defined"
#endif


int main (void) {

  for(int itr = 1; itr < 20; ++itr) {
    printf("Hello world : %d\n\r", itr);
  }

  return 1;
}

