#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# This script reads the destination folder passed as argument
# and creates an empty CMSIS-Pack directory if it does not exist
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)
# - source <CMSIS Build installation>/etc/setup

############### EDIT BELOW ###############
index_url="https://www.keil.com/pack/index.pidx"
indexfile="index.pidx"

############ DO NOT EDIT BELOW ###########
version=

# header
echo "($(basename "$0")): Setup Pack Directory $version (C) 2021 ARM"

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

# check command line for 1 argument
if [ $# -eq 1 ]; then
  # exactly 1 argument was passed..use it..its available in $1
  echo "info: reading directory: $1"
  dirname="$1"
elif [ $# -eq 0 ]; then
  # check if CMSIS_PACK_ROOT is set
  if [ -z ${CMSIS_PACK_ROOT+x} ]; then
    echo "error: no argument passed and CMSIS_PACK_ROOT environment variable not set"
    exit 1
  else
    echo "info: no argument passed - using CMSIS_PACK_ROOT environment variable:"
    echo "      $CMSIS_PACK_ROOT"
    dirname="$CMSIS_PACK_ROOT"
  fi
else
  # >1 arguments were passed...error out.
  echo "error: more than one command line argument passed"
  echo "       usage: $(basename $0) <new pack repository>"
  echo
  exit 1
fi

# check if directory already exists
if [ -d "$dirname" ]; then
  echo "error: directory "$dirname""
  echo "       already exists. Cannot create new pack repository."
  exit 1
fi

# create directory
mkdir -p "$dirname"
pushd "$dirname"
mkdir .Download
mkdir .Web
pushd ./.Web

echo "downloading package index file from $pack_url"
curl -# -k -L --output "$indexfile" "$index_url"
if [ $? -ne 0 ]; then
  echo "error: index.pidx download failed"
  popd
  popd
  rm -rf "$dirname"
  exit 1
fi

# if downloaded file is an XML file
indexfiletype=$(file "$indexfile")
if [[ "$indexfiletype" != *"XML 1.0 document"* ]]; then
  echo "error: downloaded file $indexfile is not an xml file"
  # clean-up downloaded file
  rm -f "$packfile"
  popd
  popd
  rm -rf "$dirname"
  exit 1
fi

popd
popd

if [ ! "${CMSIS_PACK_ROOT}" = "${dirname}" ]; then
  packroot="$(readlink -f "${dirname}")"
  echo "pack repository created successfully"
  echo "please setup environment variable CMSIS_PACK_ROOT to:"
  echo "\$ export CMSIS_PACK_ROOT=$packroot"
else
  echo "pack repository created successfully here:"
  echo "$CMSIS_PACK_ROOT"
fi

exit 0
