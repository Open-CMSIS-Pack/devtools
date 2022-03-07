#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2020-02-19
# This script installs the Keil.PreIncludeTestPack
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
packfile="${PWD}/../integrationtests/pack/Keil.PreIncludeTestPack.0.0.2.pack"

# construct pack folder name
remainder=($(basename $packfile))
vendor="${remainder%%.*}"; remainder="${remainder#*.}"
packname="${remainder%%.*}"; remainder="${remainder#*.}"
major="${remainder%%.*}"; remainder="${remainder#*.}"
minor="${remainder%%.*}"; remainder="${remainder#*.}"
patch="${remainder%%.*}"; remainder="${remainder#*.}"
packdir=$CMSIS_PACK_ROOT/$vendor/$packname/$major.$minor.$patch

# Install pre-include test pack
if [ -d "$packdir" ]; then
  echo "info: PreIncludeTestPack is already installed"
else
  # create pack directory
  mkdir -p "$packdir"
  pushd "$packdir"
  # extract pack into versioned pack directory
  unzip "$packfile"
  popd
  if [ $? -ne 0 ]; then
    echo "error: unzip failed for $packfile"
    # remove version directory and content as the unzip failed
    rm -rf "$packdir"
    exit 1
  fi
fi
exit 0
