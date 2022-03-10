# Build invocation with cbuild

The cbuild.sh shell script implements the build flow by chaining the utilities [cbuildgen](https://github.com/Open-CMSIS-Pack/devtools/tree/main/tools/buildmgr/cbuildgen) and CMake. It replicates the build steps of CMSIS-Pack aware IDEs and also copies configuration files from packs if necessary. The script can be adopted to project specific requirements or replaced by a custom implementation (for example a Python script).

The build flow of the `cbuild`:

1. Call [cbuildgen](https://github.com/Open-CMSIS-Pack/devtools/tree/main/tools/buildmgr/cbuildgen) with command `packlist` to list the URLs of missing software packs.
1. Call [cpackget](https://github.com/Open-CMSIS-Pack/cpackget) to download and install missing software packs.
1. Call [cbuildgen](https://github.com/Open-CMSIS-Pack/devtools/tree/main/tools/buildmgr/cbuildgen) with command `cmake` to generate a CMakeLists.txt file (if `--cmake` is specified).
1. Call `cmake` to compile the project source code into the binary image using the specified `<BuildSystem>`.

## Usage

```
(cbuild.sh): Build Invocation 0.10.4 (C) 2021 ARM
Usage:
  cbuild.sh <ProjectFile>.cprj
  [--toolchain=<Toolchain> --outdir=<OutDir> --intdir=<IntDir> <CMakeTarget>]

  <ProjectFile>.cprj      : CMSIS Project Description input file
  --toolchain=<Toolchain> : select the toolchain
  --intdir=<IntDir>       : set intermediate directory
  --outdir=<OutDir>       : set output directory
  --quiet                 : suppress output messages except build invocations
  --clean                 : remove intermediate and output directories
  --update=<CprjFile>     : generate <CprjFile> for reproducing current build
  --help                  : launch documentation and exit
  --log=<LogFile>         : save output messages in a log file
  --jobs=<N>              : number of job slots for parallel execution
  --cmake[=<BuildSystem>] : select build system, default <BuildSystem>=Ninja
  <CMakeTarget>           : optional CMake target name
```

## Example

**Cmake based build**

```
$ cbuild.sh Blinky.B-L475E-IOT01A.cprj --cmake
(cbuild.sh): Build Invocation 0.10.0 (C) 2020 ARM
Blinky.B-L475E-IOT01A.cprj validates
(cbuildgen): Build Process Manager 0.10.1-nightly+343 (C) 2020 ARM
M650: Command completed successfully.
(cbuildgen): Build Process Manager 0.10.1-nightly+343 (C) 2020 ARM
M652: Generated makefile for project build: 'C:/Blinky/B-L475E-IOT01A/Objects/CMakeLists.txt'
-- The C compiler identification is ARMClang 6.15.2
-- Configuring done
-- Generating done
-- Build files have been written to: C:/Blinky/B-L475E-IOT01A/Objects
[1/49] Building C object CMakeFiles\image.dir\C_\Users\user\AppData\Local\Arm\Packs\Keil\B-L475E-IOT01A_BSP\1.0.0\Drivers\B-L475E-IOT01\stm32l475e_iot01.o
[2/49] Building C object CMakeFiles\image.dir\C_\Users\user\AppData\Local\Arm\Packs\Keil\B-L475E-IOT01A_BSP\1.0.0\Drivers\Components\lsm6dsl\lsm6dsl.o
[3/49] Building C object CMakeFiles\image.dir\C_\Users\user\AppData\Local\Arm\Packs\Keil\B-L475E-IOT01A_BSP\1.0.0\Drivers\B-L475E-IOT01\stm32l475e_iot01_gyro.o
...
[47/49] Building C object CMakeFiles\image.dir\C_\Blinky\B-L475E-IOT01A\RTE\Device\STM32L475VGTx\system_stm32l4xx.o
[48/49] Building C object CMakeFiles\image.dir\C_\Blinky\B-L475E-IOT01A\RTE\Device\STM32L475VGTx\STCubeGenerated\Src\stm32l4xx_it.o
[49/49] Linking C executable image.axf
Program Size: Code=20968 RO-data=784 RW-data=328 ZI-data=37412
cbuild.sh finished successfully!
```

## Error messages

| Type  | Message                                                | Action                              |
|-------|--------------------------------------------------------|-------------------------------------|
| ERROR | error: missing required argument `<project>.cprj`      | See usage and correct the argument. |
| ERROR | error: project file `<project>.cprj` does not exist    | Check project file.                 |
| ERROR | error: CMSIS_BUILD_ROOT environment variable not set   | Set Environment Variables.          |
| ERROR | cmake `${output}${project}.cprj` failed!               | Check CMake error messages.         |
| INFO  | cbuild.sh finished successfully!                       | For information only.               |

## Revision History
| Version  | Description
|:---------|:----------------------------------------
| 0.10.4   | Release for alpha review