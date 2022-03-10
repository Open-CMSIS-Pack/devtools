# Command line build

For command line build with software packs, the following tools and utilities are provided:
- cbuild - Starts the overall command line build process.
- cbuildgen - Creates Make or CMakeLists.txt file and manage software layers.

## Build invocation with cbuild

cbuild.sh implements the build flow by chaining the utilities cbuildgen and CMake. It replicates the build steps of CMSIS-Pack aware IDEs and also copies configuration files from packs if necessary. The script can be adopted to project specific requirements or replaced by a custom implementation (for example a Python script).

The build flow of the cbuild.sh script is:

- Call cbuildgen with command packlist to list the URLs of missing software packs.
- Call cp_install.sh to download and install missing software packs.
- Call cbuildgen with command cmake to generate a CMakeLists.txt file (if --cmake is specified).
- Call cmake to compile the project source code into the binary image using the specified <BuildSystem>.

### Invocation

cbuild is called from the Bash command line with the following syntax:

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
| --intdir=<IntDir> | Specifies the directory for intermediate files (such as generated make files, list of missing packs ( cpinstall ), command files, object files, and dependency files). |
| <CMakeTarget> | Specifies the < target> option for CMake . |
| --quiet | Suppresses output messages except build invocations. |
| --clean | Removes intermediate and output directories. |
| --update=<CprjFile> | Generates <CprjFile> with fixed versions for reproducing the current build. |
| --help | Opens this documentation. |
| --log=<LogFile> | Saves output messages in a log file. |
| --jobs=<N> | Specifies the number of job slots for the underlying build system parallel execution. Minimum 1. |
| [–cmake=<BuildSystem>] | Selects the build system, default Ninja . |