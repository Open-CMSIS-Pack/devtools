# CMSIS Toolbox

The CMSIS Toolbox encapsulates CMSIS Package Installer, CMSIS Pack Check, CMSIS Build tools and CMSIS Project Manager binaries for all supported platforms as well as documents, schemas and templates to assist the creation and compilation of multiproject solutions based on CMSIS Packs.

Tool               | Binary         | Description
:------------------|:---------------|:-------------------------------------------------
Package Installer  | cpackget       | Install packs into local environment
Pack Check         | packchk        | Validate CMSIS packs
Project Manager    | csolution      | Validate multiproject solutions and generate CPRJs
Build Manager      | cbuildgen      | Generate CMakeLists and invoke CMake/Ninja


## Getting Started

### 1) Download CMSIS Toolbox archive

- [Linux/Windows 64/macOS](https://github.com/Open-CMSIS-Pack/devtools/releases/download/tools/toolbox/0.10.0)

### 2) Toolchain download options

- [Keil MDK IDE](http://www.keil.com/mdk5)

  Version 5.36 (Sep 2021) is the latest version of MDK supporting the CMSIS Project file format (*.cprj) export and import including export of layer information.

  [Installation Guide](http://www2.keil.com/mdk5/install)

  - Install [MDK](http://www2.keil.com/demo/eval/arm.htm) first.
  - In CMSIS Toolbox installer specify the MDK installation compiler path (c:\Keil_v5\ARM\ARMClang\bin) to setup AC6 compiler for you.

- [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads):

  Version 10-2020-q4-major (Dec. 11th 2020):
  - [Windows 32-bit ZIP](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-win32.exe)
  - [Linux x86_64 Tarball](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-x86_64-linux.tar.bz2)
  - [Mac OS X 64-bit Package](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-mac.pkg)

- [ARM Compiler Version 6](https://developer.arm.com/tools-and-software/embedded/arm-compiler/downloads/version-6) **license managed**:

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

### 3) CMake and Ninja installation

- [Download](https://cmake.org/download) and install CMake 3.18.0 or higher.
- [Download](https://github.com/ninja-build/ninja/releases) Ninja v1.10.0 or higher.

### 4) CMSIS Toolbox installation instructions

To install the CMSIS Toolbox a bash environment is required. For Windows, install for example [git for Windows](https://gitforwindows.org). Call from the bash prompt:
```
./cmsis-toolbox.sh
```
The interactive self-extracting bash installer will query the destination directory for the installation, the CMSIS Pack repository directory and the location of the toolchain binaries. For further information refer to the [CMSIS Build documentation](https://open-cmsis-pack.github.io/devtools/buildmgr/0.10.4/cbuild_install.html)

       Note: CMSIS Build requires at least one of the above toolchains.

### 5) Creating a solution in VS Code
- On VS Code install the YAML language support provided by [Red Hat](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml).

- Open a terminal and setup the bash session environment variables by sourcing the cmsis-toolbox setup file:
```
source <cmsis-toolbox-installation-folder>/etc/setup
```
- Copy the `{{SolutionName}}.csolution.yml` and `{{ProjectName}}.cproject.yml` templates from the `/cmsis-toolbox/etc/` into your project folder and rename them at your discretion.

- Edit the YAML files to select a device, add files and components. The template files have references to the YAML schemas in the first comment `#yaml-language-server`.

- Use the Package Installer to create a new pack repository, download and install packs.

- Use the Project Manager to get information from the installed packs such as device names and component identifiers, to validate the solution and eventually to generate CPRJs.

- Use the Build Manager to generate CMakeLists, invoking CMake to generate artifacts and compilation database for enabling Intellisense.
