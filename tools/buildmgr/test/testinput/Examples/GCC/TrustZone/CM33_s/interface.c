#include <arm_cmse.h>     // CMSE definitions
#include "interface.h"    // Header file with secure interface API

/* typedef for non-secure callback functions */
typedef funcptr __attribute__((cmse_nonsecure_call)) funcptr_NS;

/* Non-secure callable (entry) function */
int __attribute__((cmse_nonsecure_entry)) function1(int x) {
  return x+5; 
}

/* Non-secure callable (entry) function, calling a non-secure callback function */
int __attribute__((cmse_nonsecure_entry)) function2(funcptr callback, int x, int y) {
  funcptr_NS callback_NS; // non-secure callback function pointer
  int res;

  /* return function pointer with cleared LSB */
  callback_NS = (funcptr_NS)cmse_nsfptr_create(callback);

  res = callback_NS (x, y);

  return res;
}

