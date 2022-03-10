# CMSIS-Toolbox: Installation

Content:

- [CMSIS-Toolbox: Installation](#cmsis-toolbox-installation)
  - [Introduction](#introduction)
  - [Usage](#usage)
  - [Requirements](#requirements)
    - [Toolchain Options](#toolchain-options)
  - [Configuration](#configuration)
    - [Work with VS Code](#work-with-vs-code)
  
## Introduction

To install the CMSIS-Toolbox a bash environment is required. install for example [git for Windows](https://gitforwindows.org). Call from the bash prompt:

```
./cmsis-toolbox_0.9.0.sh
```

## Usage

The `cmsis-toolbox.sh` provides an interactive mode when invoked without parameters, but aso the following options:

```
Usage:
  cmsis-toolbox_0.9.0.sh [<option>]

  -h           : Print out version and usage
  -v           : Print out version, timestamp and git hash
  -x [<dir>]   : Extract full content into optional directory
```

Below the manual setup of the CMSIS-Toolbox is explained. The command below installs the tools into the directory `./ctools`.
```
./cmsis-toolbox.sh -x ./ctools
```

## Requirements

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

## Configuration

For the manual installation described above the configuration setup is as follows:



### Work with VS Code

VS Code is an effective environment to create CMSIS-based projects.  As [**csolution**](../../projmgr/docs/Manual/Overview.md) files are in YAML format, it is recommended to install:

- [**YAML Language Support by Red Hat**](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml).

To work with the **CMSIS-Toolbox** in VS Code use:

- **Terminal - New Terminal** to open a terminal window
- In the **Terminal** window enter the following command:
```
source <cmsis-toolbox-installation-dir>/etc/setup
```

To create a new [**csolution**](projmgr/docs/Manual/Overview.md) based CMSIS project in VS Code:

- Copy the `{{SolutionName}}.csolution.yml` and `{{ProjectName}}.cproject.yml` templates from the `<cmsis-toolbox-installation-dir/etc/` into your project directory and choose filenames at your discretion.

- Edit the YAML files to select a device, add files and components. The template files have references to the YAML schemas in the first comment `#yaml-language-server`.

- Use the Package Installer [**cpackget**](../../cpackget/docs/cpackget.md) to create a new pack repository, download and install packs.

- Use the Project Manager [**csolution**](../../projmgr/docs/Manual/Overview.md) to get information from the installed packs such as device names and component identifiers, to validate the solution and to generate the `*.CPRJ` files for compilation.

- Use the Build Manager [**cbuild**](../../projmgr/docs/cbuild.md) to generate CMakeLists, invoking CMake to generate artifacts and compilation database for enabling Intellisense.
