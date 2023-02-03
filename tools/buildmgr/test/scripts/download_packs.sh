#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2020-05-19
# This script installs required packs
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

# Install packs
packlist=${testoutdir}/pack.cpinstall
> ${packlist}
echo https://www.keil.com/pack/ARM.CMSIS.5.9.0.pack >> ${packlist}
echo https://www.keil.com/pack/Keil.ARM_Compiler.1.7.2.pack >> ${packlist}
echo http://www.keil.com/pack/Keil.STM32F4xx_DFP.2.14.0.pack >> ${packlist}
echo http://www.keil.com/pack/Keil.MDK-Middleware.7.11.1.pack >> ${packlist}
cpackget pack add -a -f ${packlist}

exit 0