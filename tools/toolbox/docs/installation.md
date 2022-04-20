# CMSIS-Toolbox: Installation

Content:

- [CMSIS-Toolbox: Installation](#cmsis-toolbox-installation)
  - [Requirements](#requirements)
    - [Toolchain Options](#toolchain-options)
  - [Installation](#installation)
  - [Usage](#usage)
  - [Configuration](#configuration)
    - [./etc/setup](#etcsetup)
      - [Git Bash Setup](#git-bash-setup)
      - [Windows Command Line Setup](#windows-command-line-setup)
    - [./etc/setup/\*.cmake](#etcsetupcmake)
  - [Using VS Code](#using-vs-code)
  - [Get Started](#get-started)
  
## Requirements

The CMSIS-Toolbox uses the CMake build system with a Ninja backend.

- [**CMake**](https://cmake.org/download) version 3.18.0 or higher.
- [**Ninja**](https://github.com/ninja-build/ninja/releases) version 1.10.0 or higher.

### Toolchain Options

The CMSIS-Toolbox works with the following toolchains. Install one or more toolchains depending on your requirements.

- [**GNU Arm Embedded Toolchain**](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads):
  - [Windows 32-bit ZIP](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-win32.exe)
  - [Linux x86_64 Tarball](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2)
  - [Mac OS X 64-bit Package](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-mac.pkg)

- [**Keil MDK**](http://www.keil.com/mdk5/install) version 5.36 or higher 

- [**ARM Compiler Version 6**](https://developer.arm.com/tools-and-software/embedded/arm-compiler/downloads/version-6) **license managed**:

  Version 6.16 (Mar. 10th 2020)
  - [Windows 32-bit Installer](https://developer.arm.com/-/media/Files/downloads/compiler/DS500-BN-00025-r5p0-18rel0.zip)

    - Download installer
    - Extract archive unzip DS500-BN-00025-r5p0-18rel0.zip
    - Run win-x86_32\setup.exe
    - Default installation path: C:\Program Files (x86)\ARMCompiler6.16\

  - [Linux x86_64 Installer](https://developer.arm.com/-/media/Files/downloads/compiler/DS500-BN-00026-r5p0-18rel0.tgz)
    - Download installer
    - Extract the archive tar -xzf DS500-BN-00026-r5p0-18rel0.tgz
    - Run install_x86_64.sh

## Installation

The [**CMSIS-Toolbox**](https://github.com/Open-CMSIS-Pack/devtools/releases) is now provided as download for Win64, Linux, and macOS in an archive file.

To setup the **CMSIS-Toolbox** on a local computer, install the content of the archive file to `<cmsis-toolbox-installation-dir>`, for example to `/c/ctools`.

> **Note:**
> 
> `cmsis-toolbox.sh` is provided for legacy reasons, but may be deprecated in future versions.

## Configuration

It is required to customize the installation for the actual setup of your development environment as described in the following:

### ./etc/setup

In file `<cmsis-toolbox-installation-dir>/etc/setup` the paths to the central pack directory (`CMSIS_PACK_ROOT`), the compiler definition files (`CMSIS_COMPILER_ROOT`), and the binaries (`CMSIS_BUILD_ROOT`) are defined.

```txt
export CMSIS_PACK_ROOT=/C/Keil/ARM/PACK
export CMSIS_COMPILER_ROOT=/c/ctools/etc
export CMSIS_BUILD_ROOT=/c/ctools/bin
```

#### Git Bash Setup

In a bash environment the following command configures the environment:

```txt
source <cmsis-toolbox-installation-dir>/etc/setup
```

#### Windows Command Line Setup

For Windows use the **System Properties** dialog and add the following **Environment Variables**:

Variable             | Value
:--------------------|:---------------
CMSIS_PACK_ROOT      | Path to the central CMSIS-Pack folder (i.e. C:\Keil\ARM\PACK)
CMSIS_COMPILER_ROOT  | Path to the CMSIS-Toolbox 'etc' directory (i.e. C:\ctools\etc)
Path                 | Add to the system path to the CMSIS-Toolbox 'bin' directory (i.e. C:\ctools\bin)

### ./etc/setup/\*.cmake

The support of the various toolchains is defined by `*.cmake` files in the directory `<cmsis-toolbox-installation-dir>/etc/setup`. The filenames have the following format:


> Note: The filenames reflect the available compiler versions on the host system.  There may be multiple files for each compiler to support different versions, for example `AC6.6.16.0.cmake` and `AC6.6.18.0.cmake`.

Each of these `*.cmake` files defines the path (`TOOLCHAIN_ROOT`) to the toolchain binaries, the file extension (`EXT`) of the executable binaries, and other compiler related parameters for the invocation. Edit the files to reflect the path as shown in the example (for `AC6`) below:

```txt
############### EDIT BELOW ###############
# Set base directory of toolchain
set(TOOLCHAIN_ROOT "C:/Keil/ARM/ARMCLANG/bin")
set(EXT .exe)
```

## Using VS Code 

VS Code is an effective environment to create CMSIS-based projects.  As [**csolution**](../../projmgr/docs/Manual/Overview.md) files are in YAML format, it is recommended to install:

- [**YAML Language Support by Red Hat**](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml).

To work with the **CMSIS-Toolbox** in VS Code use:

- **Terminal - New Terminal** to open a terminal window, you may choose as profile `bash`, `powershell`, or `Command prompt`.

- In the **Terminal** window enter the following command:

## Get Started

To create a new [**csolution**](projmgr/docs/Manual/Overview.md) based CMSIS project in VS Code:

- Copy the `{{SolutionName}}.csolution.yml` and `{{ProjectName}}.cproject.yml` templates from the `<cmsis-toolbox-installation-dir/etc/` into your project directory and choose filenames at your discretion.

- Edit the YAML files to select a device, add files and components. The template files have references to the YAML schemas in the first comment `#yaml-language-server`.

- Use the Package Installer [**cpackget**](../../cpackget/docs/cpackget.md) to create a new pack repository, download and install packs.

- Use the Project Manager [**csolution**](../../projmgr/docs/Manual/Overview.md) to get information from the installed packs such as device names and component identifiers, to validate the solution and to generate the `*.CPRJ` files for compilation.

- Use the Build Manager [**cbuild**](../../buildmgr/docs/cbuild.md) to generate CMakeLists, invoking CMake to generate artifacts and compilation database for enabling Intellisense.
