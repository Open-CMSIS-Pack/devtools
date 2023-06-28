#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2020-02-27
# This script makes the CMSIS IntegrationTests setup
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)

# header
echo "Setup Test"

# usage
usage() {
  echo "Usage:"
  echo "  setup_test.sh --binary=<cbuildgenBin> --output=<TestOutputDir>"
  echo ""
  echo "  <cbuildgenBin>  : cbuildgen binary to be tested"
  echo "  <TestOutputDir> : Test output directory"
}

# silent pushd popd
pushd () {
  command pushd "$@" > /dev/null
  if [ $? -ne 0 ]; then
    echo "error: pushd $@ failed"
    exit 1
  fi
}

popd () {
  command popd "$@" > /dev/null
  if [ $? -ne 0 ]; then
    echo "error: popd $@ failed"
    exit 1
  fi
}

# Set installation configuration
# @param1 string environment variable name
# @param2 string fully qualified path to install_config.def
setinstallconfig() {
  value=""
  if [ -z ${!1+x} ]; then
    value=""
  else
    if [[ `uname -s` != "Linux" && `uname -s` != "Darwin" ]]
      then
      # windows
      value=$(echo "/$(echo ${!1} | sed -e 's/\\/\//g' -e 's/://g')")
    else
      value=$(echo "${!1}")
    fi
  fi

  echo "${value}" >> $2
  echo "$(echo "$1" | sed -e "s/^CI_//") : ${value}"
}

# arguments
if [ $# -eq 0 ]
then
  usage
  exit 0
fi

# arguments
for i in "$@"
do
  case "$i" in
    --binary=*)
      binary="${i#*=}"
      shift
    ;;
    --output=*)
      testoutdir="${i#*=}"
      shift
    ;;
    *)
      usage
      exit 0
    ;;
  esac
done

# create cbuild installer
installdir=../../cbuildgen/installer
distdir=${testoutdir}/distribution

# check if CI_CBUILD_INSTALLER is set
if [ -z ${CI_CBUILD_INSTALLER+x} ]; then
  echo "warning: CI_CBUILD_INSTALLER is not set, create local installer"
  pushd ${installdir}
  mkdir -p "${distdir}/bin/"
  cp "${binary}" "${distdir}/bin/"
  sleep 1
  ./create_installer.sh --input=${distdir} --output=${testoutdir}
  popd
  installer="${testoutdir}/cbuild_install.sh"
else
  echo "use installer: ${CI_CBUILD_INSTALLER}"
  installer="${CI_CBUILD_INSTALLER}"
fi

install_config="${testoutdir}/install_config.def"
> ${install_config}

echo "${testoutdir}/cbuild" > ${install_config}

echo "calling cbuild_install.sh with values:"
setinstallconfig CMSIS_PACK_ROOT ${install_config}
setinstallconfig AC6_TOOLCHAIN_ROOT ${install_config}
setinstallconfig GCC_TOOLCHAIN_ROOT ${install_config}
setinstallconfig IAR_TOOLCHAIN_ROOT ${install_config}

# Run cbuild installer
dos2unix ${install_config}
"${installer}" < ${install_config}

source ${testoutdir}/cbuild/etc/setup

if [ -f "${CMSIS_PACK_ROOT}/.Web/index.pidx" ]; then
  echo "warning: directory "${CMSIS_PACK_ROOT}" already exists."
  echo "skip pack repository creation."
  ${testoutdir}/cbuild/bin/cpackget update-index
else
  ${testoutdir}/cbuild/bin/cpackget init https://www.keil.com/pack/index.pidx
fi
