#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2022 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# header
echo "Generate CMSIS-Build Debian Package"

# usage
usage() {
  echo "Usage:"
  echo "  make_deb.sh --input=<DistDir> --output=<PackageDir>"
  echo ""
  echo "  <DistDir>    : CMSIS-Build Distribution directory"
  echo "  <PackageDir> : Debian Package output directory"
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
      input="${i#*=}"
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

# check arguments
input=$(realpath ${input})
if [ -z ${input} ]
  then
  echo "error: missing required argument input=<DistDir> "
  usage
  exit 1
fi

output=$(realpath ${output})
if [ -z ${output} ]
  then
  echo "error: missing required argument output=<PackageDir> "
  usage
  exit 1
fi

# variables
PACKAGE_NAME=cmsis-build
PACKAGE_VERSION=$(echo $(${input}/bin/cbuildgen.lin-amd64) | cut -f5 -d ' ')
echo "PACKAGE_VERSION: $PACKAGE_VERSION"

rm -rf ${output}/${PACKAGE_NAME}-${PACKAGE_VERSION} > /dev/null 2>&1
mkdir -p ${output}/${PACKAGE_NAME}-${PACKAGE_VERSION}
pushd ${output}/${PACKAGE_NAME}-${PACKAGE_VERSION}
# Add files
#---------------
mkdir -p usr/bin
mkdir -p usr/lib/${PACKAGE_NAME}
mkdir -p usr/share/doc/${PACKAGE_NAME}
mkdir -p etc/${PACKAGE_NAME}
mkdir -p etc/profile.d

# Get cpackget
cpackget_version="2.1.4"
cpackget_base=https://github.com/Open-CMSIS-Pack/cpackget/releases/download/v${cpackget_version}/cpackget_${cpackget_version}
curl --retry 3 -L ${cpackget_base}_linux_amd64.tar.gz -o - | tar xzfO - --wildcards '*cpackget' > ${input}/bin/cpackget.lin-amd64

# Get cbuild2cmake
cbuild2cmake_version="0.9.3"
cbuild2cmake_base=https://github.com/Open-CMSIS-Pack/cbuild2cmake/releases/download/v${cbuild2cmake_version}/cbuild2cmake_${cbuild2cmake_version}
curl --retry 3 -L ${cbuild2cmake_base}_linux_amd64.tar.gz  -o - | tar xzfO - --wildcards '*cbuild2cmake' > ${distdir}/bin/cbuild2cmake.lin-amd64

# Get generator-bridge
cbridge_version="0.9.11"
cbridge_base=https://github.com/Open-CMSIS-Pack/generator-bridge/releases/download/v${cbridge_version}/cbridge_${cbridge_version}
curl --retry 3 -L ${cbridge_base}_linux_amd64.tar.gz -o - | tar xzf - &&\
  cp cbridge_${cbridge_version}_linux_amd64/cbridge ${distdir}/bin/cbridge.lin-amd64 &&\
  cp cbridge_${cbridge_version}_linux_amd64/launch-MCUXpressoConfigTools ${distdir}/bin/launch-MCUXpressoConfigTools.lin-amd64 &&\
  rm -r cbridge_${cbridge_version}_linux_amd64

# Get csolution
csolution_version="2.6.0"
csolution_base=https://github.com/Open-CMSIS-Pack/devtools/releases/download/tools%2Fprojmgr%2F${csolution_version}/projmgr.zip
curl --retry 3 -L ${csolution_base} -o temp.zip && unzip -q -d temp temp.zip
cp 'temp/bin/linux-amd64/csolution' ${input}/bin/csolution.lin-amd64
cp -r temp/etc/* etc/${PACKAGE_NAME}
cp -r temp/etc/* usr/lib/${PACKAGE_NAME} && rm temp.zip && rm -rf temp

# Get cbuild
cbuild_version="2.6.0"
cbuild_base=https://github.com/Open-CMSIS-Pack/cbuild/releases/download/v${cbuild_version}/cbuild_${cbuild_version}
curl --retry 3 -L ${cbuild_base}_linux_amd64.tar.gz  -o - | tar xzfO - --wildcards '*cbuild' > ${input}/bin/cbuild.lin-amd64

cp -r ${input}/bin usr/lib/${PACKAGE_NAME}  # This should be in /usr/bin but cannot for the time being.
cp -r ${input}/doc usr/share/doc/${PACKAGE_NAME}
cp -r ${input}/etc/* etc/${PACKAGE_NAME}
cp -r ${input}/etc/ usr/lib/${PACKAGE_NAME} # Remove when utilities in /bin no longer reliant on <path_to_utilities>/../etc/
# The following is not recommended but could not find a better way
# https://www.debian.org/doc/debian-policy/ch-opersys.html#environment-variables
cat > etc/profile.d/${PACKAGE_NAME}.sh << EnvironmentVariables
export CMSIS_PACK_ROOT=~/.cache/arm/packs
export CMSIS_COMPILER_ROOT=/etc/${PACKAGE_NAME}
EnvironmentVariables
find . -type f -name "*.exe*" -exec rm {} ';'
find . -type f -name "*.mac*" -exec rm {} ';'
find . -type f -name "*.lin-arm64" -exec rm {} ';'
find . -type f -name "*.lin-amd64" | sed -e 's/.lin-amd64//g' | xargs -I file mv file".lin-amd64" file
find usr/lib/${PACKAGE_NAME}/bin -type f -not -name "*.sh" | xargs -I file basename file | xargs -I util ln -s /usr/lib/${PACKAGE_NAME}/bin/util usr/bin/util
# For the bash scripts, create wrappers as suggested in documentation:
# https://www.debian.org/doc/debian-policy/ch-opersys.html#environment-variables
function write_wrapper {
cat > usr/bin/$1 << ScriptWrapper
#!/bin/bash
# Copyright (C) 2020-2022 Arm Ltd. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#
# CMSIS Build utility wrapper ($1)
source /etc/profile.d/$2.sh 
exec /usr/lib/$2/bin/$1 "\$@"
ScriptWrapper
}
export -f write_wrapper
find usr/lib/${PACKAGE_NAME}/bin -type f -name "*.sh" | xargs -I file basename file | xargs -I util bash -c 'write_wrapper "$@"' _ util ${PACKAGE_NAME}
chmod -R +x usr/bin
chmod -R +x usr/lib/${PACKAGE_NAME}/bin
chmod -R +x etc/profile.d

# Defines Documentation
#----------------------
# http://www.fifi.org/doc/doc-base/doc-base.html/ch2.html
# TODO register documents
mkdir -p usr/doc/doc-base
cat > usr/doc/doc-base/${PACKAGE_NAME} << Documentation
	Document: ${PACKAGE_NAME}
	Title: Debian ${PACKAGE_NAME} Manual
	Author: Daniel Brondani
	Abstract: This manual describes what ${PACKAGE_NAME} is
		and how it can be used to build CMSIS projects.
	Section: Apps/Programming

	Format: HTML
	Index: /usr/share/doc/${PACKAGE_NAME}/index.html
	Files: /usr/share/doc/${PACKAGE_NAME}/**/*
Documentation

# Define package
#----------------
mkdir -p debian
# Changelog
rm debian/changelog > /dev/null 2>&1
dch --create -v ${PACKAGE_VERSION} --package ${PACKAGE_NAME} --empty
# Compat
echo "7" > debian/compat
# Control
cat > debian/control << PackageDetails
Source: ${PACKAGE_NAME}
Maintainer: Daniel Brondani - Arm Ltd <daniel.brondani@arm.com>
Section: devel
Priority: optional
Standards-Version: 3.9.2
Build-Depends: debhelper (>= 9)
Homepage: https://arm-software.github.io/CMSIS_5/Build/html/index.html

Package: ${PACKAGE_NAME}
Architecture: amd64
Depends: \${shlibs:Depends}, \${misc:Depends}, cmake, ninja-build, curl, libxml2-utils, dos2unix, unzip
Description: CMSIS-Build is a set of tools, software frameworks, and workflows that improve productivity
PackageDetails
# Copyright
cat > debian/copyright << 'PackageCopyright'
Copyright (C) 2020 Arm Ltd. All rights reserved.
SPDX-License-Identifier: Apache-2.0
PackageCopyright
# Rules
cat > debian/rules << 'PackageRules'
#!/usr/bin/make -f
#based on rules defined by alien

PACKAGE=$(shell dh_listpackages)

build:
	dh_testdir

clean:
	dh_testdir
	dh_testroot
	dh_clean -d

binary-indep: build

binary-arch: build
	dh_testdir
	dh_testroot
	dh_prep
	dh_installdirs

	dh_installdocs
	dh_installchangelogs

# Copy the packages's files.
	find . -maxdepth 1 -mindepth 1 -not -name debian -print0 | \
		xargs -0 -r -i cp -a {} debian/$(PACKAGE)


# If you need to move files around in debian/$(PACKAGE) or do some
# binary patching, do it here



# This has been known to break on some wacky binaries.
#	dh_strip
	dh_compress
#	dh_fixperms
	dh_makeshlibs
	dh_installdeb
	dh_shlibdeps
	dh_gencontrol
	dh_md5sums
	dh_builddeb

binary: binary-indep binary-arch
.PHONY: build clean binary-indep binary-arch binary
PackageRules

chmod u+x debian/rules
# Source Format
#mkdir -p debian/source
#echo "3.0 (quilt)" > debian/source/format

# Build Package
#--------------
debuild -us -uc
popd
exit 0
