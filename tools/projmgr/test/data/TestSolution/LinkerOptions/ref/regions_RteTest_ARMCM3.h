#ifndef REGIONS_RTETEST_ARMCM3_H
#define REGIONS_RTETEST_ARMCM3_H


//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------
//------ With VS Code: Open Preview for Configuration Wizard -------------------

// <n> Auto-generated using information from packs
// <i> Device Family Pack (DFP):   ARM::RteTest_DFP@0.2.0

// <h> ROM Configuration
// =======================
// <h> __ROM0 (is region: IROM1 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x00000000
#define __ROM0_BASE 0x00000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00040000
#define __ROM0_SIZE 0x00040000
//   <q> Default region
//   <i> Enables memory region globally for the application.
#define __ROM0_DEFAULT 1
//   <q> Startup
//   <i> Selects region to be used for startup code.
#define __ROM0_STARTUP 1
// </h>

// </h>

// <h> RAM Configuration
// =======================
// <h> __RAM0 (is region: IRAM1 from DFP)
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x20000000
#define __RAM0_BASE 0x20000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00020000
#define __RAM0_SIZE 0x00020000
//   <q> Default region
//   <i> Enables memory region globally for the application.
#define __RAM0_DEFAULT 1
//   <q> No zero initialize
//   <i> Excludes region from zero initialization.
#define __RAM0_NOINIT 1
// </h>

// </h>

// <h> Stack / Heap Configuration
//   <o0> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
//   <o1> Heap Size (in Bytes) <0x0-0xFFFFFFFF:8>
#define __STACK_SIZE 0x00000200
#define __HEAP_SIZE 0x00000000
// </h>


#endif /* REGIONS_RTETEST_ARMCM3_H */
