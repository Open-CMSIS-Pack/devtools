#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2022 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# This script runs the installer
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)

usage() {
  echo "Usage:"
  echo "  $(basename $0) [<TestOutput> <args>]"
  echo ""
}

if [ $# -eq 0 ]; then
  echo "error: no argument passed"
  usage
  exit 1
fi

# arguments
for i in "$@"
do
  case "$i" in
    --testoutput=*)
      testoutdir="${i#*=}"
      shift
    ;;
    *)
      args+="$i "
    ;;
  esac
done

if [ ! -d "${testoutdir}" ]; then
  echo "error: directory ${testoutdir} does not exist"
  exit 1
fi

# check if CI_CBUILD_INSTALLER is set
if [ -z ${CI_CBUILD_INSTALLER+x} ]; then
  echo "error: CI_CBUILD_INSTALLER is not set"
  exit 1
fi

echo "Using installer: ${CI_CBUILD_INSTALLER}"
installer="${CI_CBUILD_INSTALLER}"

install_config=install_config.def
> ${install_config}

echo "${testoutdir}/Installation" > ${install_config}
if [[ `uname -s` != "Linux" && `uname -s` != "Darwin" ]]
  then
  # windows
  echo "/$(echo $CMSIS_PACK_ROOT | sed -e 's/\\/\//g' -e 's/://g')" >> ${install_config}
  echo "/$(echo $AC6_TOOLCHAIN_ROOT | sed -e 's/\\/\//g' -e 's/://g')" >> ${install_config}
  echo "/$(echo $GCC_TOOLCHAIN_ROOT | sed -e 's/\\/\//g' -e 's/://g')" >> ${install_config}
  echo "/$(echo $IAR_TOOLCHAIN_ROOT | sed -e 's/\\/\//g' -e 's/://g')" >> ${install_config}
else
  echo "$CMSIS_PACK_ROOT" >> ${install_config}
  echo "$AC6_TOOLCHAIN_ROOT" >> ${install_config}
  echo "$GCC_TOOLCHAIN_ROOT" >> ${install_config}
  echo "$IAR_TOOLCHAIN_ROOT" >> ${install_config}
fi
cat ${install_config}

# Run cbuild installer
dos2unix ${install_config}
"${installer}" ${args} -a "${CI_ARCH}" < ${install_config}
if [ $? -ne 0 ]
  then
  exit 1
fi

exit 0
