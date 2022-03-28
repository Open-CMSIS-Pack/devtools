# cbuild: Build invocation

**cbuild.sh** implements the build flow by chaining the utilities cbuildgen and CMake. It replicates the build steps of CMSIS-Pack aware IDEs and also copies configuration files from packs if necessary. The script can be adopted to project specific requirements or replaced by a custom implementation (for example a Python script).

The build flow of the cbuild.sh script is:

- Call cbuildgen with command packlist to list the URLs of missing software packs.
- Call cgetpack to download and install missing software packs.
- Call cbuildgen with command CMake to generate a CMakeLists.txt file.
- Call cmake to compile the project source code into the binary image using the specified <BuildSystem>.

## Usage

**cbuild.sh** is called from the Bash command line with the following syntax:

```
$ cbuild.sh <ProjectFile>.cprj [--toolchain=<Toolchain> --outdir=<OutDir> --intdir=<IntDir> --update=<CprjFile> --jobs=<N> --log=<LogFile> --quiet <CMakeTarget>]
```

Where:

cbuild.sh is the name of the script.

<ProjectFile> specifies the project file in CMSIS project format.

**Operation**

| Option | Description |
|--------|-------------|
| --toolchain=<Toolchain> | Specifies the selected toolchain for projects that support multiple compilers. |
| --outdir=<OutDir> | Specifies the output directory (for log files, binaries, and map files). |
| --intdir=<IntDir> | Specifies the directory for intermediate files (such as generated CMake files, list of missing packs, command files, object files, and dependency files). |
| --quiet | Suppresses output messages except build invocations. |
| --clean | Removes intermediate and output directories. |
| --update=<CprjFile> | Generates <CprjFile> with fixed versions for reproducing the current build. |
| --help | Prints the usage. |
| --log=<LogFile> | Saves output messages in a log file. |
| --jobs=<N> | Specifies the number of job slots for the underlying build system parallel execution (minimum 1). |
| [–cmake=<BuildSystem>] | Selects the build system, default Ninja. |
| \<CMakeTarget\> | Specifies the <target> option for CMake. |

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

