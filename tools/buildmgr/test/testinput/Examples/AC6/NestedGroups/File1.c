#ifndef GROUP0
#error "GROUP0 is not defined!"
#endif
#ifndef GROUP1
#error "GROUP1 is not defined!"
#endif
#ifdef GROUP2
#error "GROUP2 is defined!"
#endif
#ifdef GROUP3
#error "GROUP3 is defined!"
#endif
#ifndef DEF1
#error "DEF1 is not defined"
#endif
#if (DEF1 != 1)
#error "DEF1 is not equal to 1!"
#endif
