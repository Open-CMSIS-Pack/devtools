#ifndef GROUP0
#error "GROUP0 is not defined!"
#endif
#ifdef GROUP1
#error "GROUP1 is defined!"
#endif
#ifndef GROUP2
#error "GROUP2 is not defined!"
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
