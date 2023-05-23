#ifndef REGIONS_RTETEST_CM4_BOARD_H
#define REGIONS_RTETEST_CM4_BOARD_H


//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

// <n>Board pack:    ARM.RteTestBoard.0.1.0
// <i>Board pack used to generate this file

// <n>Device pack:   ARM.RteTest_DFP.0.2.0
// <i>Device pack used to generate this file

// <h>ROM Configuration
// =======================
// <h> FLASH=<__ROM0>
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x00000000
#define __ROM0_BASE 0x00000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00040000
#define __ROM0_SIZE 0x00040000
//   <q>Default region
//   <i> Enables memory region globally for the application.
#define __ROM0_DEFAULT 1
//   <q>Startup
//   <i> Selects region to be used for startup code.
#define __ROM0_STARTUP 1
// </h>

// <h> BoardFLASH (board memory)=<__ROM1>
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x30000000
#define __ROM1_BASE 0x30000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00040000
#define __ROM1_SIZE 0x00040000
//   <q>Default region
//   <i> Enables memory region globally for the application.
#define __ROM1_DEFAULT 1
//   <q>Startup
//   <i> Selects region to be used for startup code.
#define __ROM1_STARTUP 1
// </h>

// </h>

// <h>RAM Configuration
// =======================
// <h> SRAM=<__RAM0>
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x20000000
#define __RAM0_BASE 0x20000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00020000
#define __RAM0_SIZE 0x00020000
//   <q>Default region
//   <i> Enables memory region globally for the application.
#define __RAM0_DEFAULT 1
//   <q>No zero initialize
//   <i> Excludes region from zero initialization.
#define __RAM0_NOINIT 1
// </h>

// <h> BoardRAM (board memory)=<__RAM1>
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x40000000
#define __RAM1_BASE 0x40000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00020000
#define __RAM1_SIZE 0x00020000
//   <q>Default region
//   <i> Enables memory region globally for the application.
#define __RAM1_DEFAULT 1
//   <q>No zero initialize
//   <i> Excludes region from zero initialization.
#define __RAM1_NOINIT 0
// </h>

// </h>

// <h>Stack / Heap Configuration
//   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
//   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
#define __STACK_SIZE 0x00000200
#define __HEAP_SIZE 0x00000C00
// </h>


#endif /* REGIONS_RTETEST_CM4_BOARD_H */
