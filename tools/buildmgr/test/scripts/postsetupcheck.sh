#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2020-04-30
# This script checks for env vars
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

envvars=("CMSIS_PACK_ROOT" "CMSIS_COMPILER_ROOT")

errcnt=0
for var in ${envvars[@]}
  do
    if [ -z "${!var}" ]
    then
      errcnt=$((errcnt+1))
      echo "error: "$var" not available"
    fi
done

if [ $errcnt -eq 0 ]
  then
    exit 0
  else
    exit 1
fi
