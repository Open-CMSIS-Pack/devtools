
   .syntax unified
   .arch armv7-m

#ifndef _RTE_
   .equ SET_ERR_DEF,1
#endif

   .ifndef SET_ERR_DEF
   .error "SET_ERR_DEF is not defined! It seems this file was preprocessed but it shouldn't!"
   .endif

   .end
