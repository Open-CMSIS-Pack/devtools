#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2023 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

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

# Get cpackget
cpackget_version="2.1.4"
cpackget_base=https://github.com/Open-CMSIS-Pack/cpackget/releases/download/v${cpackget_version}/cpackget_${cpackget_version}
curl --retry 3 -L ${cpackget_base}_linux_amd64.tar.gz  -o - | tar xzfO - --wildcards    '*cpackget'     > ${distdir}/bin/cpackget.lin-amd64
curl --retry 3 -L ${cpackget_base}_windows_amd64.zip   -o temp.zip && unzip -p temp.zip '*/cpackget.exe' > ${distdir}/bin/cpackget.exe-amd64 && rm temp.zip
curl --retry 3 -L ${cpackget_base}_darwin_amd64.tar.gz -o - | tar xzfO - --wildcards    '*cpackget'     > ${distdir}/bin/cpackget.mac-amd64
curl --retry 3 -L ${cpackget_base}_linux_arm64.tar.gz  -o - | tar xzfO - --wildcards    '*cpackget'     > ${distdir}/bin/cpackget.lin-arm64
curl --retry 3 -L ${cpackget_base}_windows_arm64.zip   -o temp.zip && unzip -p temp.zip '*/cpackget.exe' > ${distdir}/bin/cpackget.exe-arm64 && rm temp.zip
curl --retry 3 -L ${cpackget_base}_darwin_arm64.tar.gz -o - | tar xzfO - --wildcards    '*cpackget'     > ${distdir}/bin/cpackget.mac-arm64

# Get cbuild2cmake
cbuild2cmake_version="0.9.3"
cbuild2cmake_base=https://github.com/Open-CMSIS-Pack/cbuild2cmake/releases/download/v${cbuild2cmake_version}/cbuild2cmake_${cbuild2cmake_version}
curl --retry 3 -L ${cbuild2cmake_base}_linux_amd64.tar.gz  -o - | tar xzfO - --wildcards    '*cbuild2cmake'     > ${distdir}/bin/cbuild2cmake.lin-amd64
curl --retry 3 -L ${cbuild2cmake_base}_windows_amd64.zip   -o temp.zip && unzip -p temp.zip '*/cbuild2cmake.exe' > ${distdir}/bin/cbuild2cmake.exe-amd64 && rm temp.zip
curl --retry 3 -L ${cbuild2cmake_base}_darwin_amd64.tar.gz -o - | tar xzfO - --wildcards    '*cbuild2cmake'     > ${distdir}/bin/cbuild2cmake.mac-amd64
curl --retry 3 -L ${cbuild2cmake_base}_linux_arm64.tar.gz  -o - | tar xzfO - --wildcards    '*cbuild2cmake'     > ${distdir}/bin/cbuild2cmake.lin-arm64
curl --retry 3 -L ${cbuild2cmake_base}_windows_arm64.zip   -o temp.zip && unzip -p temp.zip '*/cbuild2cmake.exe' > ${distdir}/bin/cbuild2cmake.exe-arm64 && rm temp.zip
curl --retry 3 -L ${cbuild2cmake_base}_darwin_arm64.tar.gz -o - | tar xzfO - --wildcards    '*cbuild2cmake'     > ${distdir}/bin/cbuild2cmake.mac-arm64

# Get cbridge
cbridge_version="0.9.11"
cbridge_base=https://github.com/Open-CMSIS-Pack/generator-bridge/releases/download/v${cbridge_version}/cbridge_${cbridge_version}
curl --retry 3 -L ${cbridge_base}_linux_amd64.tar.gz -o - | tar xzf - &&\
  cp cbridge_${cbridge_version}_linux_amd64/cbridge ${distdir}/bin/cbridge.lin-amd64 &&\
  cp cbridge_${cbridge_version}_linux_amd64/launch-MCUXpressoConfigTools ${distdir}/bin/launch-MCUXpressoConfigTools.lin-amd64 &&\
  rm -r cbridge_${cbridge_version}_linux_amd64
curl --retry 3 -L ${cbridge_base}_windows_amd64.zip -o temp.zip &&\
  unzip -p temp.zip '*/cbridge.exe' > ${distdir}/bin/cbridge.exe-amd64 &&\
  unzip -p temp.zip '*/launch-MCUXpressoConfigTools.bat' > ${distdir}/bin/launch-MCUXpressoConfigTools.bat-amd64 &&\
  rm temp.zip
curl --retry 3 -L ${cbridge_base}_darwin_amd64.tar.gz -o - | tar xzf - &&\
  cp cbridge_${cbridge_version}_darwin_amd64/cbridge ${distdir}/bin/cbridge.mac-amd64 &&\
  cp cbridge_${cbridge_version}_darwin_amd64/launch-MCUXpressoConfigTools ${distdir}/bin/launch-MCUXpressoConfigTools.mac-amd64 &&\
  rm -r cbridge_${cbridge_version}_darwin_amd64  
curl --retry 3 -L ${cbridge_base}_linux_arm64.tar.gz -o - | tar xzf - &&\
  cp cbridge_${cbridge_version}_linux_arm64/cbridge ${distdir}/bin/cbridge.lin-arm64 &&\
  cp cbridge_${cbridge_version}_linux_arm64/launch-MCUXpressoConfigTools ${distdir}/bin/launch-MCUXpressoConfigTools.lin-arm64 &&\
  rm -r cbridge_${cbridge_version}_linux_arm64
curl --retry 3 -L ${cbridge_base}_windows_arm64.zip -o temp.zip &&\
  unzip -p temp.zip '*/cbridge.exe' > ${distdir}/bin/cbridge.exe-arm64 &&\
  unzip -p temp.zip '*/launch-MCUXpressoConfigTools.bat' > ${distdir}/bin/launch-MCUXpressoConfigTools.bat-arm64 &&\
  rm temp.zip
curl --retry 3 -L ${cbridge_base}_darwin_arm64.tar.gz -o - | tar xzf - &&\
  cp cbridge_${cbridge_version}_darwin_arm64/cbridge ${distdir}/bin/cbridge.mac-arm64 &&\
  cp cbridge_${cbridge_version}_darwin_arm64/launch-MCUXpressoConfigTools ${distdir}/bin/launch-MCUXpressoConfigTools.mac-arm64 &&\
  rm -r cbridge_${cbridge_version}_darwin_arm64

# Get csolution
csolution_version="2.6.0"
csolution_base=https://github.com/Open-CMSIS-Pack/devtools/releases/download/tools%2Fprojmgr%2F${csolution_version}/projmgr.zip
curl --retry 3 -L ${csolution_base} -o temp.zip && unzip -q -d temp temp.zip
cp 'temp/bin/linux-amd64/csolution' ${distdir}/bin/csolution.lin-amd64
cp 'temp/bin/windows-amd64/csolution.exe' ${distdir}/bin/csolution.exe-amd64
cp 'temp/bin/darwin-amd64/csolution' ${distdir}/bin/csolution.mac-amd64
cp 'temp/bin/linux-arm64/csolution' ${distdir}/bin/csolution.lin-arm64
cp 'temp/bin/windows-arm64/csolution.exe' ${distdir}/bin/csolution.exe-arm64
cp 'temp/bin/darwin-arm64/csolution' ${distdir}/bin/csolution.mac-arm64
cp -r temp/etc/* ${distdir}/etc
rm temp.zip && rm -rf temp

# Get cbuild
cbuild_version="2.6.0"
cbuild_base=https://github.com/Open-CMSIS-Pack/cbuild/releases/download/v${cbuild_version}/cbuild_${cbuild_version}
curl --retry 3 -L ${cbuild_base}_linux_amd64.tar.gz  -o - | tar xzfO - --wildcards    '*cbuild'     > ${distdir}/bin/cbuild.lin-amd64
curl --retry 3 -L ${cbuild_base}_windows_amd64.zip   -o temp.zip && unzip -p temp.zip '*/cbuild.exe' > ${distdir}/bin/cbuild.exe-amd64 && rm temp.zip
curl --retry 3 -L ${cbuild_base}_darwin_amd64.tar.gz -o - | tar xzfO - --wildcards    '*cbuild'     > ${distdir}/bin/cbuild.mac-amd64
curl --retry 3 -L ${cbuild_base}_linux_arm64.tar.gz  -o - | tar xzfO - --wildcards    '*cbuild'     > ${distdir}/bin/cbuild.lin-arm64
curl --retry 3 -L ${cbuild_base}_windows_arm64.zip   -o temp.zip && unzip -p temp.zip '*/cbuild.exe' > ${distdir}/bin/cbuild.exe-arm64 && rm temp.zip
curl --retry 3 -L ${cbuild_base}_darwin_arm64.tar.gz -o - | tar xzfO - --wildcards    '*cbuild'     > ${distdir}/bin/cbuild.mac-arm64

arch=$(uname -m)
case $arch in
  'amd64' | 'x86_64')
    arch="amd64"
    ;;
  'arm64' | aarch64*)
    arch="arm64"
    ;;
  *)
  echo "[ERROR] Unsupported architecture $arch"
  exit 1
  ;;
esac

OS=$(uname -s)
case $OS in
  'Linux' | 'WSL_Linux')
    extn="lin-${arch}"
    ;;
  'Windows' | 'WSL_Windows'| MINGW64_NT* | MSYS_NT* | CYGWIN_NT*)
    extn="exe-${arch}"
    ;;
  'Darwin')
    # darwin arm64 native support is not available yet, temporary using amd64 instead
    extn="mac-amd64"
    ;;
  *)
    echo "[ERROR] Unsupported OS $OS"
    exit 1
    ;;
esac

PKG_VERSION=$(echo $(${distdir}/bin/cbuildgen.${extn} --version 2> /dev/null) | cut -f2 -d ' ')

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
