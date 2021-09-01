#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2020-04-28
# This script uninstalls listed Pack
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)

usage() {
  echo "Usage:"
  echo "  $(basename $0) <Test Output directory>"
  echo ""
}

# check command line for 1 argument
if [ $# -eq 1 ]; then
  echo "info: using directory: $1"
  testoutdir="$1"
elif [ $# -eq 0 ]; then
  echo "error: no argument passed"
  usage
  exit 1
else
  # >1 arguments are passed
  echo "error: more than one command line argument passed"
  usage
  exit 1
fi

if [ ! -d "${testoutdir}" ]; then
  echo "error: directory ${testoutdir} does not exist"
  exit 1
fi

# Set environment variables
source ${testoutdir}/cbuild/etc/setup

if [ -e "$CMSIS_PACK_ROOT/.Download/Keil.BulbBoard_BSP.1.1.0.pack" ]
  then
    rm -f "$CMSIS_PACK_ROOT/.Download/Keil.BulbBoard_BSP.1.1.0.pack"
fi

if [ -e "$CMSIS_PACK_ROOT/.Download/Keil.FM0plus_DFP.1.2.0.pack" ]
  then
    rm -f "$CMSIS_PACK_ROOT/.Download/Keil.FM0plus_DFP.1.2.0.pack"
fi

exit 0
