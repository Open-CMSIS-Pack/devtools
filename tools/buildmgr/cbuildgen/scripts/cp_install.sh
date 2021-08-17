#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# This bash script reads the input file passed as argument
# each line represents a url to a CMSIS Software Pack
# <ur>/<vendor>.<packname>.<version>.pack
# each pack will be downloaded to $CMSIS_PACK_ROOT/.Downloads
# each pack will be installed into $CMSIS_PACK_ROOT/<vendor>/<packname>/<version>
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)
# - source <CMSIS Build installation>/etc/setup

version=

# header
echo "($(basename "$0")): Install Packs $version (C) 2021 ARM"

# usage
usage() {
  echo "       usage: $(basename $0) <filename>"
  echo
  echo "       info: file lists one pack download link per line"
  echo "             <url>/<vendor>.<packname>.<version>.pack"
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

# check command line has 1 argument
if [ $# -eq 1 ]; then
  # exactly 1 argument was passed..use it..its available in $1
  echo "info: reading file: $1"
elif [ $# -eq 0 ]; then
  # 0 arguments were passed...error out.
  echo "error: missing command line argument"
  usage
  exit 1
else
  # more than one argument was passed...error out.
  echo "error: too many command line arguments"
  usage
  exit 1
fi

filename="$1"

# check if file exists
if [ ! -e "$filename" ]
 then
 echo "error: $filename does not exist"
 exit 1
fi

# check if CMSIS_PACK_ROOT is set
if [ -z ${CMSIS_PACK_ROOT+x} ]
 then
 echo "error: CMSIS_PACK_ROOT environment variable not set"
 exit 1
fi

# check if CMSIS_PACK_ROOT directory exists
if [ ! -e "$CMSIS_PACK_ROOT/.Web/index.pidx" ] 
  then
  echo "error: CMSIS_PACK_ROOT: "$CMSIS_PACK_ROOT" folder does not contain package index file .Web/index.pidx"
  exit 1
fi

# ensure linux file endings
dos2unix "$filename"

# read file line by line
errcnt=0
while IFS= read -r pack_url || [ -n "$pack_url" ];
  do
  # download
  pushd "$CMSIS_PACK_ROOT/.Download"
  packfile="$(basename "$pack_url")"
  if [ -e "$packfile" ]
    then
    echo "info: pack $packfile is already downloaded"
  else
    echo "$pack_url"
    curl -# -k -L --output "$packfile" "$pack_url"
    if [ $? -ne 0 ]; then
      echo "error: pack download failed for $pack_url"
      errcnt=$((errcnt+1))
      popd
      continue
    fi
    # if downloaded file is a zip file
    packfiletype="$(file "$packfile")"
    if [[ "$packfiletype" != *"Zip archive data"* ]]; then
      echo "error: downloaded file $packfile is not a zip/pack file"
      errcnt=$((errcnt+1))
      # clean-up downloaded file
      rm -f "$packfile"
      popd
      continue
    fi
  fi
  # construct pack folder name
  remainder="$packfile"
  vendor="${remainder%%.*}"; remainder="${remainder#*.}"
  packname="${remainder%%.*}"; remainder="${remainder#*.}"
  major="${remainder%%.*}"; remainder="${remainder#*.}"
  minor="${remainder%%.*}"; remainder="${remainder#*.}"
  patch="${remainder%%.*}"; remainder="${remainder#*.}"
  packdir="$CMSIS_PACK_ROOT/$vendor/$packname/$major.$minor.$patch"
  if [ -d "$packdir" ]
    then
    echo "info: $packfile is already installed"
    popd
    continue
  else
    echo "info: $packfile installing into $packdir"
    # create pack directory
    mkdir -p "$packdir"
    # extract pack into versioned pack directory
    pushd "$packdir"
    unzip -q "$CMSIS_PACK_ROOT/.Download/$packfile"
    if [ $? -ne 0 ]; then
      echo "error: unzip failed for $packfile"
      errcnt=$((errcnt+1))
      # remove version directory and content as the unzip failed
      popd
      rm -rf "$packdir"
      continue
    fi
    popd
  fi
  popd
done < "$filename"

if [ $errcnt -ne 0 ]; then
  echo "pack installation completed: ERRORS ($errcnt)"
  exit 1
else
  echo "pack installation completed successfully"
fi
