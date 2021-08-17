#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# Version: 0.10.0
#
# This scripts creates the self-extracting CMSIS Build Installer
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)

# header
echo "Generate CMSIS-Build Installer"

# usage
usage() {
  echo "Usage:"
  echo "  create_installer.sh --input=<DistDir> --output=<InstallerDir>"
  echo ""
  echo "  <DistDir>      : CMSIS-Build Distribution directory"
  echo "  <InstallerDir> : Installer output directory"
}

# update version field
update_version() {
  filepath=$1
  version=$2
  sed -e "s|version=.*|version=${version}|"\
    ${filepath} > temp.$$ && mv temp.$$ ${filepath}
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
    --input=*)
      distdir="${i#*=}"
      shift
    ;;
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

archive_name=cbuild.tar.gz
script_name=install.sh
installer_name=cbuild_install.sh

mkdir -p ${distdir}/bin
mkdir -p ${distdir}/etc

cp -R ../scripts/. ${distdir}/bin
cp -R ../config/. ${distdir}/etc
cp ../../docs/LICENSE.txt ${distdir}

OS=$(uname -s)
case $OS in
  'Linux' | 'WSL_Linux')
    extn="lin"
    ;;
  'Windows' | 'WSL_Windows'| MINGW64_NT* | MSYS_NT* | CYGWIN_NT*)
    extn="exe"
    ;;
  'Darwin')
      extn="mac"
    ;;
  *)
    echo "[ERROR] Unsupported OS $OS"
    exit 1
    ;;
esac

PKG_VERSION=$(echo $(${distdir}/bin/cbuildgen.${extn}) | cut -f5 -d ' ')
files=$(echo $(find ${distdir}/bin -name "*.sh"))
for file in ${files[@]}
  do
    update_version ${file} $PKG_VERSION
done

tar czf ${archive_name} -C ${distdir} .
if [ $? -ne 0 ]
  then
  echo "Error: compressing failed"
  exit 1
fi

cp ${script_name} script
timestamp=$(date +%Y-%m-%dT%H:%M:%S)
githash=$(git rev-parse HEAD)
sed -e "s|version=.*|version=${PKG_VERSION}|"\
    -e "s|timestamp=.*|timestamp=${timestamp}|"\
    -e "s|githash=.*|githash=${githash}|"\
    script > temp.$$ && mv temp.$$ script

mkdir -p ${output}
cat script ${archive_name} > ${output}/${installer_name}
if [ $? -ne 0 ]
  then
  echo "Error: concatenating failed"
  exit 1
fi

chmod +x ${output}/${installer_name}
rm -rf ${archive_name} script

echo "CMSIS Build Installer self-extracting ${installer_name} was created!"

exit 0