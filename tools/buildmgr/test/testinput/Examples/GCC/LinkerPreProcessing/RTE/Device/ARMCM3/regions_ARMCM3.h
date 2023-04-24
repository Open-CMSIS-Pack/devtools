#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

/*
;-------- <<< Use Configuration Wizard in Context Menu >>> -------------------
*/

/*--------------------- Flash Configuration ----------------------------------
; <h> Flash Configuration
;   <o0> ROM1 Base Address <0x0-0xFFFFFFFF:8>
;   <o1> ROM1 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> ROM2 Base Address <0x0-0xFFFFFFFF:8>
;   <o1> ROM2 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> ROM3 Base Address <0x0-0xFFFFFFFF:8>
;   <o1> ROM3 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> ROM4 Base Address <0x0-0xFFFFFFFF:8>
;   <o1> ROM4 Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>
 *----------------------------------------------------------------------------*/
#define __ROM1_BASE          0x00000000
#define __ROM1_SIZE          0x00080000
#define __ROM2_BASE          0x00000000
#define __ROM2_SIZE          0x00000000
#define __ROM3_BASE          0x00000000
#define __ROM3_SIZE          0x00000000
#define __ROM4_BASE          0x00000000
#define __ROM4_SIZE          0x00000000

/*--------------------- Embedded RAM Configuration ---------------------------
; <h> RAM Configuration
;   <o0> RAM1 Base Address    <0x0-0xFFFFFFFF:8>
;   <o1> RAM1 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> RAM2 Base Address    <0x0-0xFFFFFFFF:8>
;   <o1> RAM2 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> RAM3 Base Address    <0x0-0xFFFFFFFF:8>
;   <o1> RAM3 Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o0> RAM4 Base Address    <0x0-0xFFFFFFFF:8>
;   <o1> RAM4 Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>
 *----------------------------------------------------------------------------*/
#define __RAM1_BASE          0x20000000
#define __RAM1_SIZE          0x00040000
#define __RAM2_BASE          0x00000000
#define __RAM2_SIZE          0x00000000
#define __RAM3_BASE          0x00000000
#define __RAM3_SIZE          0x00000000
#define __RAM4_BASE          0x00000000
#define __RAM4_SIZE          0x00000000

/*--------------------- Stack / Heap Configuration ---------------------------
; <h> Stack / Heap Configuration
;   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
;   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
; </h>
 *----------------------------------------------------------------------------*/
#define __STACK_SIZE        0x00000200
#define __HEAP_SIZE         0x00000C00

/*
;------------- <<< end of configuration section >>> ---------------------------
*/

#endif
