[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/Open-CMSIS-Pack/devtools/badge)](https://securityscorecards.dev/viewer/?uri=github.com/Open-CMSIS-Pack/devtools)

# CMSIS-Pack Development Tools and Libraries

<!-- markdownlint-disable MD013 -->

This repository contains the source code of the [CMSIS-Toolbox](https://github.com/Open-CMSIS-Pack/devtools/tree/main/tools).
This toolbox provides command line tools for processing software packs that are provided in
[Open-CMSIS-Pack](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/index.html) format.

This repository provides the C++ source code of most CMSIS-Toolbox components and contains build and test configurations for Windows, Linux and macOS host platforms.

[Open-CMSIS-Pack](https://open-cmsis-pack.github.io/Open-CMSIS-Pack-Spec/main/html/index.html) defines a delivery mechanism for software components, device parameters, and evaluation board support. The XML-based package description
(`*.PDSC`) file contains the meta information of a **software pack** which
is a collection of files including:

- Source code, header files, and software libraries
- Documentation and source code templates
- Device parameters along with startup code and programming algorithms
- Example projects

The complete file collection along with the `*.PDSC` file is called **software pack** and distributed as a zip
archive using the file extension `*.pack`.

The goal of this project is to provide a consistent and compliant set of command-line tools
for **software packs** that covers the complete lifecycle including:

- creation and maintenance of software packs.
- distribution and installation of software packs.
- project build with interfaces to various compilation tools.
- interfaces for flash programming and debugging tools.

## Repository toplevel structure

```txt
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
- GNU Arm Embedded Toolchain

  | Processor | Min. Version |
  | :-- | :------ |
  | Cortex-M85 | [12.2.MPACBTI-Rel1](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) |
  | Others | [10-2020-q4-major](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) |

- A toolchain for your platform
  - **Windows:**
    - [GIT Bash](https://gitforwindows.org/)
    - Visual Studio 2019 with "Desktop development with C++"
    - CMake (minimum recommended version **3.22**)
    - *optional* make or Ninja

    ```txt
    ☑️ Make sure 'git' and 'bash' paths are listed under the PATH environment
        variable and set the git bash priority higher in the path.

    ☑️ GCC/Clang on Windows:
        Currently GCC and Clang (MSYS2/MinGW distribution) compilers do not work
        on Windows. The included libc++ has a known issue in std::filesystem,
        see [MSYS2 GitHub issue #1937](https://github.com/msys2/MSYS2-packages/issues/1937).
    ```

  - **Linux:**
    - GNU Bash (minimum recommended version **4.3**)
    - GNU Compiler (minimum recommended version **8.1**)
    - *alternatively* LLVM/Clang Compiler (minimum recommended version **8**)
    - CMake (minimum recommended version **3.22**)
    - make or Ninja
  - **MacOS:**
    - GNU Bash (minimum recommended version **4.3**)
    - XCode/AppleClang (minimum recommended version **11**)
    - CMake (minimum recommended version **3.22**)

    ```txt
    ☑️ For Apple Silicon (M1/M2 series):
        Currently the pre-installed bsdtar and apple gzip in macOS may cause some unexpected issues in CbuildIntegTests, like:
          curl: (23) Failure writing output to destination
          tar: Option --wildcards is not supported
        So please use gnu-tar and gzip:
          brew install gnu-tar
          brew install gzip
        And make sure they are added in $PATH.
    ```

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

- Create and switch to the build directory

    ```bash
    mkdir build
    cd build
    ```

### Generate configuration files

As usual, the actual build steps vary by platform.

- **Linux/MacOS amd64**:\
    On Linux or MacOS use the following commands：

    **Note:** If `DCMAKE_BUILD_TYPE` is not selected, the binaries shall build
    with `Release` configuration:

    > **cmake -DCMAKE_BUILD_TYPE=<Debug/Release> ..**

    for e.g.

    ```bash
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    ```

- **Linux cross-compiling aarch64:**\
    For cross-compiling aarch64 you need to generate with `-DCMAKE_TOOLCHAIN_FILE=../cmake/TC-linux-aarch64.cmake`:

    ```bash
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/TC-linux-aarch64.cmake ..
    ```

- **Windows:**\
    On Windows system use the following command to generate the complete workspace:

    ```bash
    cmake -A x64 ..
    ```

- **Windows cross-compiling:**\
    For cross-compiling win32 you need to generate with `-DCMAKE_TOOLCHAIN_FILE=../cmake/TC-win32-posix.cmake`:

    ```bash
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/TC-win32-posix.cmake ..
    ```

### Run build

One can trigger a build for all CMake targets or specific targets from the command line.

```txt
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

- Users can build specific target of their choice:
  - Get the list of valid CMake generated targets

    ```bash
    cat ./targets
    ```

  - Select the target
  - Build the selected target and run command
    > **cmake --build . --config <Debug/Release> --target \<target_name\>**

    for e.g.

    ```bash
    cmake --build . --config Debug --target CbuildUnitTests
    ```

## Run Tests

### Test Prerequisites

- Ensure that the above applicable [prerequistes](#prerequisites) are fulfilled.
- **Test environment setup:**\
  In order to run the tests, the test environment should know about
  - Path to [CMSIS-Pack Root Directory](https://github.com/Open-CMSIS-Pack/devtools/wiki/The-CMSIS-PACK-Root-Directory)
  - Installation path of toolchains (AC6, GCC)
  - Path to GNU Arm Embedded compiler binary

  Users can configure the test environment by setting the environment variables mentioned below.\
  **Note:** When the variables are already set, User doesn't need to set them again.

  - **CMSIS_PACK_ROOT:**
    - Follow the details [here](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/installation.md#cmsis_pack_root-this-variable-points-to-the-cmsis-pack-root-directory-that-stores-software-packs).
    - When it is pointing to an empty directory. The test scripts shall make this directory ready to be\
    used, by initializing (creating subfolder **.Download**, **.Local**, **.Web** and placing a copy of the index\
    file under **.Web/index**) this directory as a pack root directory and automatically downloading all the\
    [packs required](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/buildmgr/test/scripts/download_packs.sh#L48-L51) by test projects using [cpackget](https://github.com/Open-CMSIS-Pack/cpackget#usage) tool.

  - **GCC_TOOLCHAIN_ROOT:**\
    This variable should point to the installation path of [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads).
    - When the variable is not set. Tests shall try to find the toolchain under default path.
      | Platform    | Default path |
      | ----------- | ------------ |
      | Linux       | ${HOME}/gcc-arm-11.2-2022.02-x86_64-arm-none-eabi/bin |
      | Windows     | ${PROGRAMFILES} (x86)/Arm GNU Toolchain arm-none-eabi/11.2 2022.02/bin |
      | MacOS       | ${HOME}/gcc-arm-11.2-2022.02-x86_64-arm-none-eabi/bin |
      | WSL_Windows | ${PROGRAMFILES} (x86)/Arm GNU Toolchain arm-none-eabi/11.2 2022.02/bin |

  - **AC6_TOOLCHAIN_ROOT:**\
    This variable should point to the installation path of [Arm Compiler 6](https://developer.arm.com/tools-and-software/embedded/arm-compiler/downloads/version-6).
    - When the variable is not set. Tests shall try to find the toolchain under default path.
      | Platform    | Default path |
      | ----------- | ------------ |
      | Linux       | ${HOME}/ArmCompilerforEmbedded6.18/bin |
      | Windows     | ${PROGRAMFILES}/ArmCompilerforEmbedded6.18/bin |
      | MacOS       | ${HOME}/ArmCompilerforEmbedded6.18/bin |
      | WSL_Windows | ${PROGRAMFILES}/ArmCompilerforEmbedded6.18/bin |

  - **CC:**\
    This variable should point to the full qualified path to GNU Arm Embedded compiler binary (arm-noneabi-gcc)
    - If the variable is not set `packgen` tests shall fail.

#### Set the environment variables

  ```txt
    for e.g.
      $export CMSIS_PACK_ROOT=/path/to/Pack/Root
      $export GCC_TOOLCHAIN_ROOT=/path/to/GCC/toolchain
      $export AC6_TOOLCHAIN_ROOT=/path/to/AC6/toolchain
      $export CC=/path/to/arm-noneabi-gcc
  ```

One can directly run the tests from command line.

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

### Note

- On running the tests, all [required packs](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/buildmgr/test/scripts/download_packs.sh#L48-L51) shall get downloaded automatically by [test scripts](https://github.com/Open-CMSIS-Pack/devtools/blob/main/tools/buildmgr/test/scripts/download_packs.sh)\
 under configured pack repository.
- By default, few special tests are skipped from execution as they are dependent on specific\
 environment configuration
  or other dependencies.

    1. **CI dependent tests :**\
        These tests are designed to work only in CI (Continuous Integration) environment
        - [InstallerTests](./tools/buildmgr/test/integrationtests/src/InstallerTests.cpp)
        - [DebPkgTests](./tools/buildmgr/test/integrationtests/src/DebPkgTests.cpp)
    2. **AC6 toolchain test :**\
        The below listed tests depend on a valid AC6 toolchain installed and can be run in\
         the local environment on
        the installation of valid
        [Arm Compiler 6](https://developer.arm.com/tools-and-software/embedded/arm-compiler/downloads/version-6).
        - [CBuildAC6Tests](./tools/buildmgr/test/integrationtests/src/CBuildAC6Tests.cpp)
        - [MultiTargetAC6Tests](./tools/buildmgr/test/integrationtests/src/MultiTargetAC6Tests.cpp)

    Make sure you have the proper <!-- markdownlint-disable-next-line MD013 -->
    **[Arm Compilers licenses](https://developer.arm.com/tools-and-software/software-development-tools/license-management/resources/product-and-toolkit-configuration)**.

## Code coverage

Users can generate coverage reports locally using a GNU tool [**lcov**](https://wiki.documentfoundation.org/Development/Lcov).

### Prerequisite

Coverage reports can only be generated on **linux** platform.

- Ensure that the [linux prerequisite](#prerequisites) are fulfilled.
- Install **lcov**

  ```bash
  sudo apt-get install lcov
  ```

### Generate coverage report

- Create and switch to build directory

  ```bash
  mkdir build
  cd build
  ```

  ```txt
  ☑️ Ensure that the build tree is clean and doesn't have any existing coverage data i.e. .gcda or .gcno files
  ```

- Generate configuration files with coverage flag **ON**

  ```bash
  cmake -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON ..
  ```

- Build target tests
  > cmake --build . --config Debug --target **<target_name>**

  for e.g.

  ```bash
  cmake --build . --config Debug --target CbuildUnitTests
  ```

- Run tests
  > ctest -R **\<regex\>** -C Debug

  for e.g.

  ```bash
  ctest -R CbuildUnitTests -C Debug
  ```

- Collect coverage data
  > lcov -c --directory **\<path_to_user_space\>** --output-file **\<cov_out_file\>**

  for e.g.

  ```bash
  lcov -c --directory ./tools/buildmgr --output-file full_coverage.info
  ```

- Extract coverage data from file

  ```txt
  ☑️ By default, lcov collects coverage data also from the currently running Linux
      kernel. Specify -e option to extract data from files matching PATTERN from file
  ```

  > lcov -e **\<input_file\>** **'\<PATTERN\>'** -o **\<out_file\>**

  for e.g.

  ```bash
  lcov -e full_coverage.info '/tools/buildmgr/cbuild/*' '*/tools/buildmgr/cbuildgen/*' '*/tools/buildmgr/cbuild/*' -o coverage.info
  ```

- Generate html coverage report
  > genhtml **<cov_file>** --output-directory **<report_out_dir>**

  for e.g.

  ```bash
  genhtml coverage.info --output-directory coverage
  ```

The coverage report i.e. **index.html** is generated into the specified directory.

## Build Documentation

Some components provide Doxygen-based documentation which needs to be generated before
it can be viewed and published. There are specific build targets for these generated
documentations, see build target suffix `-docs`.

```txt
☑️ Note:
   The *-docs build targets require doxygen to be available. If CMake fails to
   detect the correct version of doxygen a warning message appears and the build
   targets are skipped.
```

To build the documentation for a specific component run the following in the
CMake build directory:

```bash
cmake --build . --target buildmgr-docs
```

The documentation is generated into the CMake binary directory of the enclosing
component.

## Visual Studio Code Dev Containers

Using Visual Studio Code in combination with Docker one can use Dev Containers to build,
run and debug, instead of using a local toolchain installation,
see <https://code.visualstudio.com/docs/devcontainers/containers>.

Reference devcontainers are defined in `.devcontainer` folder. To use them from within
Code click the *remote window* button on the lower left, choose *Open folder in container*,
and select one of the configurations from the list.

On the first use the Docker image is built from the configurations `Dockerfile`. Once the
image is cached, a new container is launched and the Code window is connected to it. The
workspace is mounted into the container so that one can work with the files right way.

It is also possible to spawn multiple instances of code connected to different dev containers
in parallel. All containers share the same workspace with the host. This way, one can run
the build using one configuration and run/debug the binaries with another system
configuration.

## License

Open-CMSIS-Pack is licensed under Apache 2.0.
