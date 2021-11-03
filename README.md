# CMSIS-Pack Development Tools and Libraries

This repository contains the source code of command line tools and library
components for processing meta information specified by
[CMSIS-Pack](https://arm-software.github.io/CMSIS_5/develop/Pack/html/index.html)
and
[CMSIS-Build](https://arm-software.github.io/CMSIS_5/develop/Build/html/index.html)
which are written in C++ as well as build and test configurations targeting
Windows, Linux and macOS host platforms.

CMSIS-Pack defines a delivery mechanism for software components, device
parameters, and evaluation board support. The XML-based package description
(PDSC) file contains the meta information of a so called 'Software Pack' which
is a collection of files including:

- Source code, header files, and software libraries
- Documentation and source code templates
- Device parameters along with startup code and programming algorithms
- Example projects

The complete file collection along with the PDSC file is distributed as a zip
archive using the file extension `*.pack`.

CMSIS-Build defines an XML project file format that allows building projects
based on hardware descriptions, software components, source modules and linker
scripts distributed as software packs. The build manager (cbuildgen) is reading
the project (`*.cprj) and generates the build instructions as CMake
configuration files.

The end goal of this project is to provide a consistent and compliant set of command-line tools
for covering the Software Pack lifecycle from creation and maintenance, via distribution and
installation all the way to the project build, flash programming and debugging on the targeted
microcontroller.

## Repository toplevel structure

```
    📦
    ┣ 📂cmake       local cmake functions and configuration files
    ┣ 📂docs        documentation shared by all components
    ┣ 📂external    3rd party components loaded as submodules
    ┣ 📂libs        reusable C++ library component source code shared by tools
    ┣ 📂scripts     scripts used by validation, build and test actions
    ┣ 📂test        test related files shared across tool and library components
    ┗ 📂tools       command line tool source code
```

## Building the tools locally

This section contains steps to generate native makefiles and workspaces that
can be used in the compiler environment of your choice.

The instructions contain a complete guide to get you the project build on
your local machine for development and testing purposes.

## Prerequisites

The following applications are required to be installed on your
machine to allow components in this repository to be built and run.

Note that some of the required tools are platform dependent:

- [Git](https://git-scm.com/)
- A toolchain for your platform
  - **Windows:**
    - [GIT Bash](https://gitforwindows.org/)
    - Visual Studio 2017 or 2019
    - CMake (minimum recommended version **3.18**)
    - *optional* make or Ninja

    ```
    ☑️ Note:
        Make sure 'git' and 'bash' paths are listed under the PATH environment
        variable and set the git bash priority higher in the path.
    ```
    
    ~~~
    ☑️ GCC/Clang on Windows:
        Currently GCC and Clang (MSYS2/MinGW distribution) compilers do not work
        on Windows. The included libc++ has a known issue in std::filesystem,
        see [MSYS2 GitHub issue #1937](https://github.com/msys2/MSYS2-packages/issues/1937).
    ~~~

  - **Linux**
    - GNU Bash (minimum recommended version **4.3**)
    - GNU Compiler (minimum recommended version **8.1**)
    - *alternatively* LLVM/Clang Compiler (minimum recommended version **8**)
    - CMake (minimum recommended version **3.18**)
    - make or Ninja
  - **MacOS**
    - GNU Bash (minimum recommended version **4.3**)
    - XCode/AppleClang (minimum recommended version **11**)
    - CMake (minimum recommended version **3.18**)
  - **General:**
    - [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) (version **10-2020-q4-major**)

    ~~~
    ☑️ Required only for packgen tests.

    Set below mentioned environment variable:
      * CC : Full qualified path to GNU Arm Embedded compiler binary (arm-noneabi-gcc)

      for e.g.
        $export CC=/c/my/path/to/arm-noneabi-gcc
    ~~~

## Clone repository

Clone github repository to create a local copy on your computer to make
it easier to develop and test. Cloning of repository can be done by following
the below git command:

```bash
git clone git@github.com:Open-CMSIS-Pack/devtools.git
```

## Build components

This is a three step process:

1. Download third party software components specified as git submodules.
2. Generate configuration files for a build system
3. Run build

### Download third party software components

- Go to `<path>/<to>/devtools` and run `git` command:

    ```bash
    git submodule update --init --recursive
    ```

- Create and enter the build directory

    ```bash
    mkdir build
    cd build
    ```

### Generate configuration files

As usual, the actual build steps vary by platform.

- **Linux/MacOS**:\
    On Linux or MacOS use the following commands.\

    **Note:** If `DCMAKE_BUILD_TYPE` is not selected, the binaries shall build
    with `Release` configuration:

    > **cmake -DCMAKE_BUILD_TYPE=<Debug/Release> ..**

    for e.g.

    ```bash
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    ```

- **Windows:**\
    On Windows system use the following command to generate the complete workspace:

    ```bash
    cmake -A x64 ..
    ```

### Run build

One can trigger a build for all CMake targets or specific targets from the command line.

```
☑️ Note:
    The flag `--config` is optional. If it is not specified in the command, depending on
    the platform the binaries shall build in default configurations.
        - Windows: Debug
        - Linux/macOS: Release
```

Follow the respective commands:

- Build all CMake targets
    > **cmake --build . --config <Debug/Release>**
    for e.g.

    ```bash
    cmake --build . --config Debug
    ```

- Users can build specific target of their choice and build:
  - Select the CMake generated target from the list below:
    - cbuild
    - cbuildgen
    - cbuildgenlib
    - CbuildUnitTests
    - CbuildIntegTests
    - CrossPlatform
    - CrossPlatformUnitTests
    - ErrLog
    - ErrLogUnitTests
    - packchk
    - packchklib
    - PackChkUnitTests
    - PackChkIntegTests
    - packgen
    - packgenlib
    - PackGenUnitTests
    - RteFsUtils
    - RteFsUtilsUnitTests
    - RteModel
    - RteModelUnitTests
    - RteUtils
    - RteUtilsUnitTests
    - XmlReader
    - XmlReaderUnitTests
    - XmlTree
    - XmlTreeUnitTests
    - XmlTreeSlim
    - XmlTreeSlimUnitTests

  - Build the selected target and run command
    > **cmake --build . --config <Debug/Release> --target \<target_name\>**

    for e.g.

    ```bash
    cmake --build . --config Debug --target CbuildUnitTests
    ```

## Run Tests

One can directly run the tests from command line.

~~~
☑️ Required only when the pack repository/installed toolchains reside in places different from the default values.

Set below mentioned environment variables:
  * CI_PACK_ROOT          : Directory that contains the software packs in CMSIS-Pack format
  * CI_GCC_TOOLCHAIN_ROOT : GCC toolchain installation path

  for e.g.
    $export CI_PACK_ROOT=/c/my/path/to/Pack/Root
    $export CI_GCC_TOOLCHAIN_ROOT=/c/my/path/to/toolchain
~~~

- Using `ctest`:\
  Use the command below to trigger the tests.
  - `ctest -C <config>`           : Run all registered tests (Note: Running all the tests can take a while)
  - `ctest -R <regex> -C <config>`: Run all tests matching the regex

  for e.g.

  ```bash
  ctest -C Debug
      or
  ctest -R CbuildUnitTests -C Debug
  ```

- Using test executable:
  - Go to root build directory
    > cd \<**root**\>
  - Run the executable
    > ./\<**path_to_executable**\>/<**executable_name**>

    for e.g.

    ```bash
    cd build
    ./tools/buildmgr/test/integrationtests/windows64/Debug/CbuildIntegTests.exe
    ```

## Note

By default, few special tests are skipped from execution as they are dependent on specific environment configuration or other dependencies.

1. **CI dependent tests :**\
    These tests are designed to work only in CI (Continuous Integration) environment
    - [InstallerTests](./tools/buildmgr/test/integrationtests/src/InstallerTests.cpp)
    - [DebPkgTests](./tools/buildmgr/test/integrationtests/src/DebPkgTests.cpp)
2. **AC6 toolchain test :**\
    The below listed tests depend on a valid AC6 toolchain installed and can be run in the local environment on the installation of valid [Arm Compiler 6](https://developer.arm.com/tools-and-software/embedded/arm-compiler/downloads/version-6).
    - [CBuildAC6Tests](./tools/buildmgr/test/integrationtests/src/CBuildAC6Tests.cpp)
    - [MultiTargetAC6Tests](./tools/buildmgr/test/integrationtests/src/MultiTargetAC6Tests.cpp)

    ~~~
    ☑️ Required only when the installed AC6 toolchain resides in places different from the default values.

    Set below mentioned environment variables:
    * CI_ARMCC6_TOOLCHAIN_ROOT : AC6 toolchain installation path

      for e.g.
        $export CI_ARMCC6_TOOLCHAIN_ROOT=/c/my/path/to/AC6/toolchain
    ~~~

    Make sure you have the proper **[Arm Compilers licenses](https://developer.arm.com/tools-and-software/software-development-tools/license-management/resources/product-and-toolkit-configuration)**.

## License

Open-CMSIS-Pack is licensed under Apache 2.0.

## Contributions and Pull Requests

Contributions are accepted under Apache 2.0. Only submit contributions where you have authored all of the code.

### Proposals, Reviews and Project

Please feel free to raise an [issue on GitHub](https://github.com/Open-CMSIS-Pack/Open-CMSIS-Pack/issues)
to start the discussion about your proposal.

We are in the early days and discuss proposals which we are dividing into 5 work streams with a dedicated label:

- **Core Library Components** - common libraries that are re-used across a range of tools. The PoC Tools are the first consumers, but the library components can also be used to create commercial derivatives or in-house tooling.
- **Overall Project Concept** - procedures to generate packs and application software. We shall consider complex applications such as multi-core processor systems or secure/non-secure partitions.
- **PoC Tools** - command line tools and utilities that implement the overall concepts and are intended to be used for open-source projects or even integrated into commercial software tools.
- **Process Improvements** - documentation and tools that help the software community to streamline and secure the software delivery to the user base.
- **Resource Management** - describes the data models used to manage and organized software packs and application projects.

These Issues are tracked inside the [project board](https://github.com/Open-CMSIS-Pack/Open-CMSIS-Pack/projects/1)

### Issues, Labels

Please feel free to raise an [issue on GitHub](https://github.com/Open-CMSIS-Pack/Open-CMSIS-Pack/issues)
to report misbehavior (i.e. bugs)

Issues are your best way to interact directly with the maintenance team and the community.
We encourage you to append implementation suggestions as this helps to decrease the
workload of the very limited maintenance team.

We shall be monitoring and responding to issues as best we can.
Please attempt to avoid filing duplicates of open or closed items when possible.
In the spirit of openness we shall be tagging issues with the following:

- **bug** – We consider this issue to be a bug that shall be investigated.

- **wontfix** - We appreciate this issue but decided not to change the current behavior.

- **out-of-scope** - We consider this issue loosely related to CMSIS. It might be implemented outside of CMSIS. Let us know about your work.

- **question** – We have further questions about this issue. Please review and provide feedback.

- **documentation** - This issue is a documentation flaw that shall be improved in the future.

- **DONE** - We consider this issue as resolved - please review and close it. In case of no further activity, these issues shall be closed after a week.

- **duplicate** - This issue is already addressed elsewhere, see a comment with provided references.
