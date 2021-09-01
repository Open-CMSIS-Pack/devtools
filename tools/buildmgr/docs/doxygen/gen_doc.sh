#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 1.0
# Date: 2021-07-26
# This bash script generates CMSIS Documentation:
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)
# - doxygen 1.8.6

# header
echo "Generate CMSIS-Build Documentation"

# usage
usage() {
  echo "Usage:"
  echo "  gen_doc.sh --output=<OutputDir>"
  echo ""
  echo "  <OutputDir> : Output directory"
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
    --output=*)
      output="${i#*=}"
      shift
    ;;
    *)
      usage
      exit 0
    ;;
  esac
done

set -o pipefail

DOXYGEN=$(which doxygen)
DOXYFILE="Build/Build.dxy"

# Create output directory
mkdir -p "${output}"
export OUTPUT_DIRECTORY=$(readlink -f ${output})

# Copy index.html
cp ./index.html ${OUTPUT_DIRECTORY}/

if [[ ! -f "${DOXYGEN}" ]]; then
    echo "Doxygen not found!" >&2
    echo "Did you miss to add it to PATH?"
    exit 1
else
    version=$("${DOXYGEN}" --version)
    echo "DOXYGEN is ${DOXYGEN} at version ${version}"
    if [[ "${version}" != "1.8.6" ]]; then
        echo " >> Version is different from 1.8.6 !" >&2
    fi
fi

echo "Generating documentation ..."

pushd $(dirname ${DOXYFILE}) > /dev/null
echo "${DOXYGEN} ${DOXYFILE}"
"${DOXYGEN}" $(basename ${DOXYFILE})
popd > /dev/null

mkdir -p "${OUTPUT_DIRECTORY}/html/search/"
cp -f "Doxygen_Templates/search.css" "${OUTPUT_DIRECTORY}/html/search/"

projectName=$(grep -E "PROJECT_NAME\s+=" ${DOXYFILE} | sed -r -e 's/[^"]*"([^"]+)"/\1/')
projectNumber=$(grep -E "PROJECT_NUMBER\s+=" ${DOXYFILE} | sed -r -e 's/[^"]*"([^"]+)"/\1/')
datetime=$(date -u +'%a %b %e %Y %H:%M:%S')
sed -e "s/{datetime}/${datetime}/" "Doxygen_Templates/cmsis_footer.js" \
| sed -e "s/{projectName}/${projectName}/" \
| sed -e "s/{projectNumber}/${projectNumber}/" \
> "${OUTPUT_DIRECTORY}/html/cmsis_footer.js"

echo "Documentation saved in ${OUTPUT_DIRECTORY}"

exit 0
