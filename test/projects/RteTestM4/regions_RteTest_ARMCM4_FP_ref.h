#ifndef REGIONS_RTETEST_ARMCM4_FP_H
#define REGIONS_RTETEST_ARMCM4_FP_H


//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

// <h>ROM Configuration
// =======================
// <h> FLASH
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

// </h>

// <h>RAM Configuration
// =======================
// <h> SRAM
//   <o> Base address <0x0-0xFFFFFFFF:8>
//   <i> Defines base address of memory region.
//   <i> Default: 0x20000000
#define __ROM0_BASE 0x20000000
//   <o> Region size [bytes] <0x0-0xFFFFFFFF:8>
//   <i> Defines size of memory region.
//   <i> Default: 0x00020000
#define __ROM0_SIZE 0x00020000
//   <q>Default region
//   <i> Enables memory region globally for the application.
#define __ROM0_DEFAULT 1
//   <q>No zero initialize
//   <i> Excludes region from zero initialization.
#define __ROM0_NOINIT 0
// </h>

// </h>


#endif /* REGIONS_RTETEST_ARMCM4_FP_H */
