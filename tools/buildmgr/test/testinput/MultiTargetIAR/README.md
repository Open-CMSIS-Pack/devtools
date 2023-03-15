# Multitarget test for EWARM
This suite aims to test the description defined by [CPRJ.xsd](../../../cbuildgen/config/CPRJ.xsd). A test is added by adding a new *.cprj file with the name Multitarget.[CORENAME_MODIFIERS].cprj.

The current testsuite tests, MODIFIER in parenthesis:
- FPU-settings (NP,SP,DP)
- debug option (DEBUG)
- warnings options (WARNINGS)
- Trust Zones (TZ)
- Vector Extensions (MVE)
- DSP (DSP)

Once a the testfile has been added alongside the others, add a section to the expectedcommands.txt which lists the expected commands on the command line for the compiler. The test-suite will try building the project and will also check that the listed command line options has been included for the compilation of MyMain.c.
