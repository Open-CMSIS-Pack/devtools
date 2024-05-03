#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2022 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# This self-extracting bash script installs CMSIS Build
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)

############### EDIT BELOW ###############

############ DO NOT EDIT BELOW ###########

version=
timestamp=
githash=

# header
echo "("$(basename "$0")"): CMSIS Build Installer $version (C) 2021-2024 ARM"

# usage
usage() {
  echo "Version: $version"
  echo "Usage:"
  echo "  $(basename $0) [<option>]"
  echo ""
  echo "  -h           : Print out version and usage"
  echo "  -v           : Print out version, timestamp and git hash"
  echo "  -x [<dir>]   : Extract full content into optional <dir>"
  echo "  -a [<dir>]   : Set architecture: amd64 or arm64"
}

# version
version() {
  echo "Version: $version"
  echo "Timestamp: $timestamp"
  echo "Git Hash: $githash"
}

# Gets the absolute path to a given file
# $1 : relative filename
# The absolute path is 'returned' on stdout for this function
get_abs_filename() {
  echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}

# Convert Windows to Unix path (alternative to 'cygpath')
unixpath() {
  # add leading forward slash, replace backslashes and remove first colon
  echo "/$(echo "$@" | sed -e 's/\\/\//g' -e 's/://')"
}

# Convert Unix to Windows path with generic directory separator
winpath() {
  norm=$(echo "$(echo "$1" | sed -e 's/\\/\//g')")
  if [ ${norm:0:1} = '/' ] && [ ${norm:2:1} = '/' ]
    then
    echo "$(echo "$norm" | sed -e 's/\///' -e 's/\//:\//')"
  else
    echo "$norm"
  fi
}

# Remove binaries
remove_bin() {
  rm -f "$@"/*/*.lin-*
  rm -f "$@"/*/*.exe-*
  rm -f "$@"/*/*.mac-*
}

# find __ARCHIVE__ maker
marker=$(awk '/^__ARCHIVE__/ {print NR + 1; exit 0; }' "${0}")

# -h argument
if [ $# -gt 0 ]
  then
  if [ $1 == "-h" ]
    then
    usage
    exit 0
  elif [ $1 == "-v" ]
    then
    version
    exit 0
  elif [ $1 == "-x" ]
    then
    if [ -z "$2" ]
      then
      dir=.
    else
      dir=$2
      mkdir -p "$dir"
    fi
    tail -n+${marker} "${0}" | tar xz -C "$dir"
    exit 0
  elif [ $1 == "-a" ]
    then
    arch=$2
  else
    echo "Warning: command line argument ignored!"
  fi
fi

# detect architecture
if [ -z ${arch} ]
  then
  arch=$(uname -m)
fi
case $arch in
  'amd64' | 'x86_64')
    arch="amd64"
    ;;
  'arm64' | aarch64*)
    arch="arm64"
    ;;
  *)
  echo "[ERROR] Unsupported architecture $arch"
  exit 1
  ;;
esac
echo "[INFO] Installing for architecture: ${arch}"

# detect platform and set 'OS' variable accordingly
OS=$(uname -s)
case $OS in
  'Linux')
    if [[ $(grep Microsoft <<< $(uname -a)) ]]; then
      echo "[INFO] Windows Subsystem for Linux (WSL) platform detected"
      read -r -p "Do you want to use Windows toolchains? [Y/n]: " response
      if [[ ! "$response" =~ ^([nN][oO]|[nN])$ ]]; then
        OS=WSL_Windows
      else
        OS=WSL_Linux
      fi
    else
      echo "[INFO] Linux platform detected"
    fi
    ;;
  'WindowsNT' | MINGW64_NT* | MSYS_NT* | CYGWIN_NT*)
    echo "[INFO] Windows platform detected"
    OS=Windows
    ;;
  'Darwin')
    echo "[INFO] Mac platform detected"
    OS=Darwin
    ;;
  *)
    echo "[ERROR] Unsupported OS $OS"
    exit 1
    ;;
esac

# The OS variable will now contain one of the following:
# Linux, Windows, Darwin, WSL_Windows or WSL_Linux
#
#
case $OS in
  'Windows')
    # windows
    install_dir_default_path=./cbuild
    cmsis_pack_root_default_path=$(unixpath "${LOCALAPPDATA}/Arm/Packs")
    cmsis_compiler_root_default_path=$(unixpath "${LOCALAPPDATA}/Arm/Compilers")
    compiler6_default_path=$(unixpath "${PROGRAMFILES}/ArmCompilerforEmbedded6.18/bin")
    gcc_default_path=$(unixpath "${PROGRAMFILES} (x86)/Arm GNU Toolchain arm-none-eabi/11.2 2022.02/bin")
    iar_default_path=$(unixpath "${PROGRAMFILES} (x86)/IAR Systems/Embedded Workbench 8.4/arm/bin")
    extension=".exe"
    ;;
  'WSL_Windows')
    # wsl with windows tools
    install_dir_default_path=./cbuild
    systemdrive=$(cmd.exe /c "echo|set /p=%SYSTEMDRIVE%")
    localappdata=$(cmd.exe /c "echo|set /p=%LOCALAPPDATA%")
    programfiles=$(cmd.exe /c "echo|set /p=%PROGRAMFILES%")
    cmsis_pack_root_default_path=$(wslpath "${localappdata}\Arm\Packs")
    cmsis_compiler_root_default_path=$(wslpath "${localappdata}\Arm\Compilers")
    compiler6_default_path=$(wslpath "${programfiles}/ArmCompilerforEmbedded6.18/bin")
    gcc_default_path=$(wslpath "${programfiles} (x86)/Arm GNU Toolchain arm-none-eabi/11.2 2022.02/bin")
    iar_default_path=$(wslpath "${programfiles} (x86)/IAR Systems/Embedded Workbench 8.4/arm/bin")
    extension=".exe"
    ;;
  'Linux' | 'Darwin' | WSL_Linux)
    # linux/macos/wsl with linux tools
    install_dir_default_path=./cbuild
    cmsis_pack_root_default_path=${HOME}/.cache/arm/packs
    cmsis_compiler_root_default_path=${HOME}/.cache/arm/compilers
    compiler6_default_path=${HOME}/ArmCompilerforEmbedded6.18/bin
    gcc_default_path=${HOME}/gcc-arm-11.2-2022.02-x86_64-arm-none-eabi/bin
    iar_default_path=/opt/iarsystems/bxarm/arm/bin
    extension=""
    ;;
esac

# user input
read -e -p "Enter base directory for CMSIS command line build tools [$install_dir_default_path]: " install_dir
install_dir=${install_dir:-$install_dir_default_path}

# ask for pack root directory
read -e -p "Enter the CMSIS_PACK_ROOT directory [$cmsis_pack_root_default_path]: " cmsis_pack_root
cmsis_pack_root=${cmsis_pack_root:-$cmsis_pack_root_default_path}
if [[ -d "${cmsis_pack_root}" ]]
  then
  cmsis_pack_root=$(get_abs_filename "${cmsis_pack_root}")
else
  echo "Warning: ${cmsis_pack_root} does not exist!"
fi

# ask for AC6 compiler installation path
read -e -p "Enter the installed Arm Compiler 6.18 directory [${compiler6_default_path}]: " compiler6_root
compiler6_root=${compiler6_root:-${compiler6_default_path}}
if [[ -d "${compiler6_root}" ]]
  then
  compiler6_root=$(get_abs_filename "${compiler6_root}")
else
  echo "Warning: ${compiler6_root} does not exist!"
fi

# ask for gcc installation path
read -e -p "Enter the installed GNU Arm Embedded Toolchain Version 10.3.1 directory [${gcc_default_path}]: " gcc_root
gcc_root=${gcc_root:-${gcc_default_path}}
if [[ -d "${gcc_root}" ]]
  then
  gcc_root=$(get_abs_filename "${gcc_root}")
else
  echo "Warning: ${gcc_root} does not exist!"
fi

# ask for IAR installation path
read -e -p "Enter the installed IAR C/C++ Compiler for ARM directory [${iar_default_path}]: " iar_root
iar_root=${iar_root:-${iar_default_path}}
if [[ -d "${iar_root}" ]]
  then
  iar_root=$(get_abs_filename "${iar_root}")
else
  echo "Warning: ${iar_root} does not exist!"
fi

# create install folder
mkdir -p "${install_dir}"
if [ $? -ne 0 ]
  then
  echo "Error: ${install_dir} directory cannot be created!"
  exit 1
fi

# install it
# get install_dir full path
install_dir=$(get_abs_filename "${install_dir}")
echo "Installing CMSIS Build to ${install_dir}..."

# set cmsis_compiler_root
cmsis_compiler_root="${install_dir}"/etc

# create cmsis_compiler_root folder
mkdir -p "${cmsis_compiler_root}"
if [ $? -ne 0 ]
  then
  echo "Error: ${cmsis_compiler_root} directory cannot be created!"
  exit 1
fi

# decompress archive
tail -n+${marker} "${0}" | tar xz -C "${install_dir}"
if [ $? -ne 0 ]
  then
  echo "Error: extracting files failed!"
  exit 1
fi

# manage os specific files and extensions
case $OS in
  'Linux' | 'WSL_Linux')
    extn="lin-${arch}"
    for f in "${install_dir}"/bin/*.${extn}; do
      mv "$f" "${f%.${extn}}"
    done
    remove_bin ${install_dir}
    chmod -R +x "${install_dir}"/bin
    ;;
  'Windows' | 'WSL_Windows')
    extn="exe-${arch}"
    for f in "${install_dir}"/bin/*.${extn}; do
      mv "$f" "${f%.${extn}}.exe"
    done
    echo "install_dir: ${install_dir}"
    remove_bin ${install_dir}
    ;;
  'Darwin')
    # darwin arm64 native support is not available yet, temporary using amd64 instead
    extn="mac-amd64"
    for f in "${install_dir}"/bin/*.${extn}; do
      mv "$f" "${f%.${extn}}"
    done
    remove_bin ${install_dir}
    chmod -R +x "${install_dir}"/bin
    ;;
esac

# update environment variables in etc/setup file
# Note not using in-place editing in sed as it is not portable
setup="${install_dir}"/etc/setup
sed -e "s|export CMSIS_PACK_ROOT.*|export CMSIS_PACK_ROOT=${cmsis_pack_root}|"\
    -e "s|export CMSIS_COMPILER_ROOT.*|export CMSIS_COMPILER_ROOT=${cmsis_compiler_root// /\\\\ }|"\
    "${setup}" > temp.$$ && mv temp.$$ "${setup}"

# setup WSL_Windows
if [[ $OS == "WSL_Windows" ]]
  then
  echo -e '\n# Windows Subsystem for Linux (WSL) environment variables and shell functions'\
          '\nexport WSLENV=CMSIS_PACK_ROOT/p:CMSIS_COMPILER_ROOT/p'\
          '\ncbuildgen() { cbuildgen.exe "$@" ; }'\
          '\nexport -f cbuildgen'\
          '\nccmerge() { ccmerge.exe "$@" ; }'\
          '\nexport -f ccmerge'\
          '\nmake() { make.exe "$@" ; }'\
          '\nexport -f make' >> "${setup}"

  # Toolchains shall have windows paths
  compiler6_root=$(wslpath -m "${compiler6_root}")
  gcc_root=$(wslpath -m "${gcc_root}")
  iar_root=$(wslpath -m "${iar_root}")
fi

if [[ $OS == "Windows" ]]
  then
  compiler6_root=$(winpath "${compiler6_root}")
  gcc_root=$(winpath "${gcc_root}")
  iar_root=$(winpath "${iar_root}")
fi

# update toolchain config files
script="${cmsis_compiler_root}/AC6.6.16.2.cmake"
sed -e "s|set(TOOLCHAIN_ROOT.*|set(TOOLCHAIN_ROOT \"${compiler6_root}\")|" "${script}" > temp.$$ && mv temp.$$ "${script}"
sed -e "s|set(EXT.*|set(EXT ${extension})|" "${script}" > temp.$$ && mv temp.$$ "${script}"

script="${cmsis_compiler_root}/GCC.10.3.1.cmake"
sed -e "s|set(TOOLCHAIN_ROOT.*|set(TOOLCHAIN_ROOT \"${gcc_root}\")|" "${script}" > temp.$$ && mv temp.$$ "${script}"
sed -e "s|set(EXT.*|set(EXT ${extension})|" "${script}" > temp.$$ && mv temp.$$ "${script}"

script="${cmsis_compiler_root}/IAR.9.32.1.cmake"
sed -e "s|set(TOOLCHAIN_ROOT.*|set(TOOLCHAIN_ROOT \"${iar_root}\")|" "${script}" > temp.$$ && mv temp.$$ "${script}"
sed -e "s|set(EXT.*|set(EXT ${extension})|" "${script}" > temp.$$ && mv temp.$$ "${script}"

echo "CMSIS Build installation completed!"
echo "To setup the bash environment run:"
echo "\$ source "$install_dir"/etc/setup"
exit 0

__ARCHIVE__
